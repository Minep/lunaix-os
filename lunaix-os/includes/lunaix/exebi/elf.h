/**
 * @file elf.h
 * @author Lunaixsky (lunaxisky@qq.com)
 * @brief 
 * @version 0.1
 * @date 2024-07-05
 * 
 * Define generic structure that described in Executable and Linking Format
 * specification.
 * 
 * Define interface on manipulate such structure
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef __LUNAIX_ELF32_H
#define __LUNAIX_ELF32_H

#include <lunaix/types.h>
#include <sys/exebi/elf.h>

#define ET_EXEC 2
#define ET_DYN 3

#define PT_LOAD 1
#define PT_INTERP 3

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

#define EV_CURRENT 1

// [0x7f, 'E', 'L', 'F']
#define ELFMAGIC_LE 0x464c457fU

#define EI_CLASS 4
#define EI_DATA 5

#define NO_LOADER 0
#define DEFAULT_LOADER "usr/ld"

struct elf_ehdr;
struct elf_phdr;

struct elf
{
    const void* elf_file;
    struct elf_ehdr eheader;
    struct elf_phdr* pheaders;
};

#define declare_elf32(elf, elf_vfile)                                          \
    struct elf elf = { .elf_file = elf_vfile, .pheaders = (void*)0 }

int
elf_check_exec(const struct elf* elf, int type);

int
elf_check_arch(const struct elf* elf);

int
elf_open(struct elf* elf, const char* path);

int
elf_openat(struct elf* elf, void* elf_vfile);

int
elf_static_linked(const struct elf* elf);

int
elf_close(struct elf* elf);

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
elf_find_loader(const struct elf* elf, char* path_out, size_t len);

int
elf_read_ehdr(struct elf* elf);

int
elf_read_phdr(struct elf* elf);

/**
 * @brief Estimate how much memeory will be acquired if we load all loadable
 * sections.、
 *
 * @param elf
 * @return size_t
 */
size_t
elf_loadable_memsz(const struct elf* elf);

#define SIZE_EHDR sizeof(struct elf_ehdr)
#define SIZE_PHDR sizeof(struct elf_phdr)

#endif /* __LUNAIX_ELF32_H */
