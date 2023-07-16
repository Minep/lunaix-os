#include <lunaix/common.h>
#include <lunaix/exebi/elf32.h>
#include <lunaix/fs.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

#include <klibc/string.h>

static inline int
elf32_read(struct v_file* elf, void* data, size_t off, size_t len)
{
    // it is wise to do cached read
    return pcache_read(elf->inode, data, len, off);
}

int
elf32_open(struct elf32* elf, const char* path)
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

    return elf32_openat(elf, elffile);
}

int
elf32_openat(struct elf32* elf, const void* elf_vfile)
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

    return 0;
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

    return 0;
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
            if (len >= phdre->p_filesz) {
                return EINVAL;
            }

            retval =
              elf32_read(elfile, path_out, phdre->p_offset, phdre->p_filesz);

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

    return elf32_read(elfile, (void*)&elf->eheader, 0, SIZE_EHDR);
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

    status = elf32_read(elfile, phdrs, elf->eheader.e_phoff, tbl_sz);

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
    const struct elf32_ehdr* ehdr = &elf->eheader;

    return *(u32_t*)(ehdr->e_ident) == ELFMAGIC &&
           ehdr->e_ident[EI_CLASS] == ELFCLASS32 &&
           ehdr->e_ident[EI_DATA] == ELFDATA2LSB && ehdr->e_type == ET_EXEC &&
           ehdr->e_machine == EM_386;
}
