#include <lunaix/common.h>
#include <lunaix/elf.h>
#include <lunaix/fs.h>
#include <lunaix/ld.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>

int
__elf_populate_mapped(struct mm_region* region, void* pg, off_t offset)
{
    struct elf32_phdr* phdr = (struct elf32_phdr*)region->data;
    size_t segsz = phdr->p_filesz;
    size_t segoff = offset - phdr->p_offset;

    if (segoff >= segsz) {
        return 0;
    }

    struct v_file* file = region->mfile;
    size_t rdlen = MIN(segsz - segoff, PG_SIZE);

    if (rdlen == PG_SIZE) {
        // This is because we want to exploit any optimization on read_page
        return file->ops->read_page(file->inode, pg, PG_SIZE, offset);
    } else {
        return file->ops->read(file->inode, pg, rdlen, offset);
    }
}

void
__elf_destruct_mapped(struct mm_region* region)
{
    vfree(region->data);
}

int
elf_map_segment(struct ld_param* ldparam,
                struct v_file* elfile,
                struct elf32_phdr* phdr)
{
    int proct = 0;
    if ((phdr->p_flags & PF_R)) {
        proct |= PROT_READ;
    }
    if ((phdr->p_flags & PF_W)) {
        proct |= PROT_WRITE;
    }
    if ((phdr->p_flags & PF_X)) {
        proct |= PROT_EXEC;
    }

    struct mm_region* seg_reg;
    struct mmap_param param = { .vms_mnt = ldparam->vms_mnt,
                                .regions = &ldparam->proc->mm.regions,
                                .proct = proct,
                                .offset = phdr->p_offset,
                                .length = ROUNDUP(phdr->p_memsz, PG_SIZE),
                                .flags =
                                  MAP_FIXED | MAP_PRIVATE | REGION_TYPE_CODE };

    int status = mem_map(NULL, &seg_reg, PG_ALIGN(phdr->p_va), elfile, &param);

    if (!status) {
        struct elf32_phdr* phdr_ = valloc(sizeof(SIZE_PHDR));
        *phdr_ = *phdr;
        seg_reg->data = phdr;

        seg_reg->init_page = __elf_populate_mapped;
        seg_reg->destruct_region = __elf_destruct_mapped;
    }

    return status;
}

int
elf_setup_mapping(struct ld_param* ldparam,
                  struct v_file* elfile,
                  struct elf32_ehdr* ehdr)
{
    int status = 0;
    size_t tbl_sz = ehdr->e_phnum * SIZE_PHDR;
    struct elf32_phdr* phdrs = valloc(tbl_sz);

    if (!phdrs) {
        status = ENOMEM;
        goto done;
    }

    tbl_sz = 1 << ILOG2(tbl_sz);
    phdrs = elfile->ops->read(elfile->inode, phdrs, tbl_sz, ehdr->e_phoff);

    size_t entries = tbl_sz / SIZE_PHDR;
    for (size_t i = 0; i < entries; i++) {
        struct elf32_phdr* phdr = &phdrs[i];

        if (phdr->p_type == PT_LOAD) {
            status = elf_map_segment(ldparam, elfile, phdr);
        }
        // TODO process other types of segments

        if (status) {
            ldparam->status |= LD_STAT_FKUP;
            goto done;
        }
    }

done:
    vfree(phdrs);
    return status;
}

int
elf_load(struct ld_param* ldparam, struct v_file* elfile)
{
    struct elf32_ehdr* ehdr = valloc(SIZE_EHDR);
    int status = elfile->ops->read(elfile->inode, ehdr, SIZE_EHDR, 0);

    if (status) {
        goto done;
    }

    if (!elf_check_exec(ehdr)) {
        status = ENOEXEC;
        goto done;
    }

    if ((status = elf_setup_mapping(ldparam, elfile, ehdr))) {
        goto done;
    }

    ldparam->ehdr_out = *ehdr;

done:
    vfree(ehdr);
    return status;
}