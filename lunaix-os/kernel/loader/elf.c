#include <lunaix/common.h>
#include <lunaix/elf.h>
#include <lunaix/fs.h>
#include <lunaix/ld.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>

int
elf_map_segment(struct ld_param* ldparam,
                struct v_file* elfile,
                struct elf32_phdr* phdr)
{
    assert(PG_ALIGNED(phdr->p_offset));

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

    struct mmap_param param = { .vms_mnt = ldparam->vms_mnt,
                                .pvms = &ldparam->proc->mm,
                                .proct = proct,
                                .offset = PG_ALIGN(phdr->p_offset),
                                .mlen = ROUNDUP(phdr->p_memsz, PG_SIZE),
                                .flen = phdr->p_filesz + PG_MOD(phdr->p_va),
                                .flags = MAP_FIXED | MAP_PRIVATE,
                                .type = REGION_TYPE_CODE };

    struct mm_region* seg_reg;
    int status = mem_map(NULL, &seg_reg, PG_ALIGN(phdr->p_va), elfile, &param);

    if (!status) {
        size_t next_addr = phdr->p_memsz + phdr->p_va;
        ldparam->info.end = MAX(ldparam->info.end, ROUNDUP(next_addr, PG_SIZE));
        ldparam->info.mem_sz += phdr->p_memsz;
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
    status = elfile->ops->read(elfile->inode, phdrs, tbl_sz, ehdr->e_phoff);

    if (status < 0) {
        goto done;
    }

    if (PG_ALIGN(phdrs[0].p_va) != USER_START) {
        status = ENOEXEC;
        goto done;
    }

    size_t entries = tbl_sz / SIZE_PHDR;
    for (size_t i = 0; i < entries; i++) {
        struct elf32_phdr* phdr = &phdrs[i];

        if (phdr->p_type == PT_LOAD) {
            if (phdr->p_align == PG_SIZE) {
                status = elf_map_segment(ldparam, elfile, phdr);
            } else {
                // surprising alignment!
                status = ENOEXEC;
            }
        }
        // TODO process other types of segments

        if (status) {
            // errno in the middle of mapping restructuring, it is impossible
            // to recover!
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

    if (status < 0) {
        goto done;
    }

    if (!elf_check_exec(ehdr)) {
        status = ENOEXEC;
        goto done;
    }

    if ((status = elf_setup_mapping(ldparam, elfile, ehdr))) {
        goto done;
    }

    ldparam->info.ehdr_out = *ehdr;

done:
    vfree(ehdr);
    return status;
}