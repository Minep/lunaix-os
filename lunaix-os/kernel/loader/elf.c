#include <klibc/string.h>
#include <lunaix/common.h>
#include <lunaix/elf.h>
#include <lunaix/exec.h>
#include <lunaix/fs.h>
#include <lunaix/mm/mmap.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>

static inline int
elf32_read(struct v_file* elf, void* data, size_t off, size_t len)
{
    // it is wise to do cached read
    return pcache_read(elf->inode, data, len, off);
}

int
elf32_map_segment(struct load_context* ldctx,
                  const struct elf32* elf,
                  struct elf32_phdr* phdre)
{
    struct v_file* elfile = (struct v_file*)elf->elf_file;

    assert(PG_ALIGNED(phdre->p_offset));

    int proct = 0;
    if ((phdre->p_flags & PF_R)) {
        proct |= PROT_READ;
    }
    if ((phdre->p_flags & PF_W)) {
        proct |= PROT_WRITE;
    }
    if ((phdre->p_flags & PF_X)) {
        proct |= PROT_EXEC;
    }

    struct exec_container* container = ldctx->container;
    struct mmap_param param = { .vms_mnt = container->vms_mnt,
                                .pvms = &container->proc->mm,
                                .proct = proct,
                                .offset = PG_ALIGN(phdre->p_offset),
                                .mlen = ROUNDUP(phdre->p_memsz, PG_SIZE),
                                .flen = phdre->p_filesz + PG_MOD(phdre->p_va),
                                .flags = MAP_FIXED | MAP_PRIVATE,
                                .type = REGION_TYPE_CODE };

    struct mm_region* seg_reg;
    int status = mem_map(NULL, &seg_reg, PG_ALIGN(phdre->p_va), elfile, &param);

    if (!status) {
        size_t next_addr = phdre->p_memsz + phdre->p_va;
        ldctx->end = MAX(ldctx->end, ROUNDUP(next_addr, PG_SIZE));
        ldctx->mem_sz += phdre->p_memsz;
    } else {
        // we probably fucked up our process
        terminate_proc(-1);
    }

    return status;
}

int
elf32_open(struct elf32* elf, const char* path)
{
    struct v_dnode* elfdn;
    struct v_inode* elfin;
    struct v_file* elffile;
    int error = 0;

    if ((error = vfs_walk_proc(path, &elfdn, NULL, 0))) {
        return error;
    }

    if ((error = vfs_open(elfdn, &elffile))) {
        return error;
    }

    return elf32_openat(elf, elffile);
}

int
elf32_openat(struct elf32* elf, void* elf_vfile)
{
    int status = 0;
    elf->pheaders = NULL;
    elf->elf_file = elf_vfile;

    if ((status = elf32_read_ehdr(elf)) < 0) {
        elf32_close(elf);
        return status;
    }

    if ((status = elf32_read_phdr(elf)) < 0) {
        elf32_close(elf);
        return status;
    }

    return status;
}

int
elf32_close(struct elf32* elf)
{
    if (elf->pheaders) {
        vfree(elf->pheaders);
    }

    if (elf->elf_file) {
        vfs_close((struct v_file*)elf->elf_file);
    }

    memset(elf, 0, sizeof(*elf));
}

int
elf32_static_linked(const struct elf32* elf)
{
    for (size_t i = 0; i < elf->eheader.e_phnum; i++) {
        struct elf32_phdr* phdre = &elf->pheaders[i];
        if (phdre->p_type == PT_INTERP) {
            return 0;
        }
    }
    return 1;
}

size_t
elf32_loadable_memsz(const struct elf32* elf)
{
    // XXX: Hmmmm, I am not sure if we need this. This is designed to be handy
    // if we decided to map the heap region before transfer to loader. As
    // currently, we push *everything* to user-space loader, thus we modify the
    // brk syscall to do the initial heap mapping.

    size_t sz = 0;
    for (size_t i = 0; i < elf->eheader.e_phnum; i++) {
        struct elf32_phdr* phdre = &elf->pheaders[i];
        if (phdre->p_type == PT_LOAD) {
            sz += phdre->p_memsz;
        }
    }

    return sz;
}

int
elf32_find_loader(const struct elf32* elf, char* path_out, size_t len)
{
    int retval = NO_LOADER;

    assert_msg(len >= sizeof(DEFAULT_LOADER), "path_out: too small");

    struct v_file* elfile = (struct v_file*)elf->elf_file;

    for (size_t i = 0; i < elf->eheader.e_phnum; i++) {
        struct elf32_phdr* phdre = &elf->pheaders[i];
        if (phdre->p_type == PT_INTERP) {
            assert_msg(len >= phdre->p_filesz, "path_out: too small");
            retval =
              elf_read(elfile, path_out, phdre->p_offset, phdre->p_filesz);

            if (retval < 0) {
                return retval;
            }

            break;
        }
    }

    return retval;
}

int
elf32_read_ehdr(struct elf32* elf)
{
    struct v_file* elfile = (struct v_file*)elf->elf_file;
    int status = elf_read(elfile, (void*)&elf->eheader, 0, SIZE_EHDR);

    if (status < 0) {
        return status;
    }
}

int
elf32_read_phdr(struct elf32* elf)
{
    int status = 0;

    struct v_file* elfile = (struct v_file*)elf->elf_file;

    size_t entries = elf->eheader.e_phnum;
    size_t tbl_sz = entries * SIZE_PHDR;

    struct elf32_phdr* phdrs = valloc(tbl_sz);

    if (!phdrs) {
        return ENOMEM;
    }

    status = elf_read(elfile, phdrs, elf->eheader.e_phoff, tbl_sz);

    if (status < 0) {
        vfree(phdrs);
        return status;
    }

    elf->pheaders = phdrs;
    return entries;
}

int
elf32_check_exec(const struct elf32* elf)
{
    struct elf32_ehdr* ehdr = elf->pheaders;

    return *(u32_t*)(ehdr->e_ident) == ELFMAGIC &&
           ehdr->e_ident[EI_CLASS] == ELFCLASS32 &&
           ehdr->e_ident[EI_DATA] == ELFDATA2LSB && ehdr->e_type == ET_EXEC &&
           ehdr->e_machine == EM_386;
}

int
elf32_load(struct load_context* ldctx, const struct elf32* elf)
{
    int err = 0;

    struct v_file* elfile = (struct v_file*)elf->elf_file;

    for (size_t i = 0; i < elf->eheader.e_phnum && !err; i++) {
        struct elf32_phdr* phdr = &elf->pheaders[i];

        if (phdr->p_type == PT_LOAD) {
            if (phdr->p_align != PG_SIZE) {
                // surprising alignment!
                err = ENOEXEC;
                continue;
            }

            err = elf_map_segment(ldctx, elf, phdr);
        }
        // TODO Handle relocation
    }

done:
    return err;
}