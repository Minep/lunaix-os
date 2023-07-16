#ifndef __LUNAIX_ELF32_H
#define __LUNAIX_ELF32_H

#include <lunaix/types.h>

typedef unsigned int elf32_ptr_t;
typedef unsigned short elf32_hlf_t;
typedef unsigned int elf32_off_t;
typedef unsigned int elf32_swd_t;
typedef unsigned int elf32_wrd_t;

#define ET_NONE 0
#define ET_EXEC 2

#define PT_LOAD 1
#define PT_INTERP 3

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

#define EM_NONE 0
#define EM_386 3

#define EV_CURRENT 1

// [0x7f, 'E', 'L', 'F']
#define ELFMAGIC 0x464c457fU
#define ELFCLASS32 1
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

#define EI_CLASS 4
#define EI_DATA 5

#define NO_LOADER 0
#define DEFAULT_LOADER "usr/ld"

struct elf32_ehdr
{
    u8_t e_ident[16];
    elf32_hlf_t e_type;
    elf32_hlf_t e_machine;
    elf32_wrd_t e_version;
    elf32_ptr_t e_entry;
    elf32_off_t e_phoff;
    elf32_off_t e_shoff;
    elf32_wrd_t e_flags;
    elf32_hlf_t e_ehsize;
    elf32_hlf_t e_phentsize;
    elf32_hlf_t e_phnum;
    elf32_hlf_t e_shentsize;
    elf32_hlf_t e_shnum;
    elf32_hlf_t e_shstrndx;
};

struct elf32_phdr
{
    elf32_wrd_t p_type;
    elf32_off_t p_offset;
    elf32_ptr_t p_va;
    elf32_ptr_t p_pa;
    elf32_wrd_t p_filesz;
    elf32_wrd_t p_memsz;
    elf32_wrd_t p_flags;
    elf32_wrd_t p_align;
};

struct elf32
{
    const void* elf_file;
    struct elf32_ehdr eheader;
    struct elf32_phdr* pheaders;
};

#define declare_elf32(elf, elf_vfile)                                          \
    struct elf32 elf = { .elf_file = elf_vfile, .pheaders = (void*)0 }

int
elf32_check_exec(const struct elf32* elf);

int
elf32_open(struct elf32* elf, const char* path);

int
elf32_openat(struct elf32* elf, const void* elf_vfile);

int
elf32_static_linked(const struct elf32* elf);

int
elf32_close(struct elf32* elf);

/**
 * @brief Try to find the PT_INTERP section. If found, copy it's content to
 * path_out
 *
 * @param elf Opened elf32 descriptor
 * @param path_out
 * @param len size of path_out buffer
 * @return int
 */
int
elf32_find_loader(const struct elf32* elf, char* path_out, size_t len);

int
elf32_read_ehdr(struct elf32* elf);

int
elf32_read_phdr(struct elf32* elf);

/**
 * @brief Estimate how much memeory will be acquired if we load all loadable
 * sections.、
 *
 * @param elf
 * @return size_t
 */
size_t
elf32_loadable_memsz(const struct elf32* elf);

#define SIZE_EHDR sizeof(struct elf32_ehdr)
#define SIZE_PHDR sizeof(struct elf32_phdr)

#endif /* __LUNAIX_ELF32_H */
