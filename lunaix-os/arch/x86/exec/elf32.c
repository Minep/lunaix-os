#include <lunaix/exebi/elf.h>

#define EM_386 3

int
elf_check_arch(const struct elf* elf)
{
    const struct elf_ehdr* ehdr = &elf->eheader;

    return *(u32_t*)(ehdr->e_ident) == ELFMAGIC_LE &&
           ehdr->e_ident[EI_CLASS] == ELFCLASS32 &&
           ehdr->e_ident[EI_DATA] == ELFDATA2LSB && ehdr->e_machine == EM_386;
}