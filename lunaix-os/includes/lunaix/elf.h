#ifndef __LUNAIX_ELF_H
#define __LUNAIX_ELF_H

#include <lunaix/types.h>

typedef unsigned int elf32_ptr_t;
typedef unsigned short elf32_hlf_t;
typedef unsigned int elf32_off_t;
typedef unsigned int elf32_swd_t;
typedef unsigned int elf32_wrd_t;

#define ET_NONE 0
#define ET_EXEC 2

#define PT_LOAD 1

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

#define EM_NONE 0
#define EM_386 3

#define EV_CURRENT 1

#define ELFMAGIC 0x464c457fU
#define ELFCLASS32 1
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

#define EI_CLASS 4
#define EI_DATA 5

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

#define SIZE_EHDR sizeof(struct elf32_ehdr)
#define SIZE_PHDR sizeof(struct elf32_phdr)

static inline int
elf_check_exec(struct elf32_ehdr* ehdr)
{
    return (*(u32_t*)(ehdr->e_ident) == ELFMAGIC) ||
           ehdr->e_ident[EI_CLASS] == ELFCLASS32 ||
           ehdr->e_ident[EI_DATA] == ELFDATA2LSB || ehdr->e_type == ET_EXEC;
}
#endif /* __LUNAIX_ELF_H */
