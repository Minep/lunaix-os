#include <lunaix/exebi/elf.h>
#include <lunaix/fs.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

#include <klibc/string.h>

static inline int
elf_read(struct v_file* elf, void* data, size_t off, size_t len)
{
    // it is wise to do cached read
    return pcache_read(elf->inode, data, len, off);
}

static int
elf_do_open(struct elf* elf, struct v_file* elf_file)
{
    int status = 0;
    elf->pheaders = NULL;
    elf->elf_file = elf_file;

    if ((status = elf_read_ehdr(elf)) < 0) {
        elf_close(elf);
        return status;
    }

    if ((status = elf_read_phdr(elf)) < 0) {
        elf_close(elf);
        return status;
    }

    return 0;
}

_default int
elf_open(struct elf* elf, const char* path)
{
    struct v_dnode* elfdn;
    struct v_file* elffile;
    int error = 0;

    if ((error = vfs_walk_proc(path, &elfdn, NULL, 0))) {
        return error;
    }

    if ((error = vfs_open(elfdn, &elffile))) {
        return error;
    }

    return elf_do_open(elf, elffile);
}

_default int
elf_openat(struct elf* elf, void* elf_vfile)
{
    // so the ref count kept in sync
    vfs_ref_file(elf_vfile);
    return elf_do_open(elf, elf_vfile);
}

_default int
elf_close(struct elf* elf)
{
    if (elf->pheaders) {
        vfree(elf->pheaders);
    }

    if (elf->elf_file) {
        vfs_close((struct v_file*)elf->elf_file);
    }

    memset(elf, 0, sizeof(*elf));

    return 0;
}

_default int
elf_static_linked(const struct elf* elf)
{
    for (size_t i = 0; i < elf->eheader.e_phnum; i++) {
        struct elf_phdr* phdre = &elf->pheaders[i];
        if (phdre->p_type == PT_INTERP) {
            return 0;
        }
    }
    return 1;
}

_default size_t
elf_loadable_memsz(const struct elf* elf)
{
    // XXX: Hmmmm, I am not sure if we need this. This is designed to be handy
    // if we decided to map the heap region before transfer to loader. As
    // currently, we push *everything* to user-space loader, thus we modify the
    // brk syscall to do the initial heap mapping.

    size_t sz = 0;
    for (size_t i = 0; i < elf->eheader.e_phnum; i++) {
        struct elf_phdr* phdre = &elf->pheaders[i];
        if (phdre->p_type == PT_LOAD) {
            sz += phdre->p_memsz;
        }
    }

    return sz;
}

_default int
elf_find_loader(const struct elf* elf, char* path_out, size_t len)
{
    int retval = NO_LOADER;

    assert_msg(len >= sizeof(DEFAULT_LOADER), "path_out: too small");

    struct v_file* elfile = (struct v_file*)elf->elf_file;

    for (size_t i = 0; i < elf->eheader.e_phnum; i++) {
        struct elf_phdr* phdre = &elf->pheaders[i];
        if (phdre->p_type == PT_INTERP) {
            if (len >= phdre->p_filesz) {
                return EINVAL;
            }

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

_default int
elf_read_ehdr(struct elf* elf)
{
    struct v_file* elfile = (struct v_file*)elf->elf_file;

    return elf_read(elfile, (void*)&elf->eheader, 0, SIZE_EHDR);
}

_default int
elf_read_phdr(struct elf* elf)
{
    int status = 0;

    struct v_file* elfile = (struct v_file*)elf->elf_file;

    size_t entries = elf->eheader.e_phnum;
    size_t tbl_sz = entries * SIZE_PHDR;

    struct elf_phdr* phdrs = valloc(tbl_sz);

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

_default int
elf_check_exec(const struct elf* elf, int type)
{
    const struct elf_ehdr* ehdr = &elf->eheader;

    return (ehdr->e_entry) && ehdr->e_type == type;
}

_default int
elf_check_arch(const struct elf* elf)
{
    const struct elf_ehdr* ehdr = &elf->eheader;

    return *(u32_t*)(ehdr->e_ident) == ELFMAGIC_LE;
}