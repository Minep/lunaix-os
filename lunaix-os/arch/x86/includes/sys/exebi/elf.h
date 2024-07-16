#ifndef __LUNAIX_ARCH_ELF_H
#define __LUNAIX_ARCH_ELF_H

#include <lunaix/types.h>

#define ELFCLASS32 1
#define ELFCLASS64 2

#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

#ifdef CONFIG_ARCH_X86_64
typedef unsigned long   elf_ptr_t;
typedef unsigned short  elf_hlf_t;
typedef unsigned long   elf_off_t;
typedef          int    elf_swd_t;
typedef unsigned int    elf_wrd_t;
typedef unsigned long   elf_xwrd_t;
typedef          long   elf_sxwrd_t;
#else
typedef unsigned int    elf_ptr_t;
typedef unsigned short  elf_hlf_t;
typedef unsigned int    elf_off_t;
typedef unsigned int    elf_swd_t;
typedef unsigned int    elf_wrd_t;
#endif

struct elf_ehdr
{
    u8_t e_ident[16];
    elf_hlf_t e_type;
    elf_hlf_t e_machine;
    elf_wrd_t e_version;
    elf_ptr_t e_entry;
    elf_off_t e_phoff;
    elf_off_t e_shoff;
    elf_wrd_t e_flags;
    elf_hlf_t e_ehsize;
    elf_hlf_t e_phentsize;
    elf_hlf_t e_phnum;
    elf_hlf_t e_shentsize;
    elf_hlf_t e_shnum;
    elf_hlf_t e_shstrndx;
};

struct elf_phdr
{
#ifdef CONFIG_ARCH_X86_64
    elf_wrd_t p_type;
    elf_wrd_t p_flags;
    elf_off_t p_offset;
    elf_ptr_t p_va;
    elf_ptr_t p_pa;
    elf_xwrd_t p_filesz;
    elf_xwrd_t p_memsz;
    elf_xwrd_t p_align;
#else
    elf_wrd_t p_type;
    elf_off_t p_offset;
    elf_ptr_t p_va;
    elf_ptr_t p_pa;
    elf_wrd_t p_filesz;
    elf_wrd_t p_memsz;
    elf_wrd_t p_flags;
    elf_wrd_t p_align;
#endif
};

#endif /* __LUNAIX_ARCH_ELF_H */
