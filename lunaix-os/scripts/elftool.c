#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

typedef unsigned long   elf64_ptr_t;
typedef unsigned short  elf64_hlf_t;
typedef unsigned long   elf64_off_t;
typedef          int    elf64_swd_t;
typedef unsigned int    elf64_wrd_t;
typedef unsigned long   elf64_xwrd_t;
typedef          long   elf64_sxwrd_t;

typedef unsigned int    elf32_ptr_t;
typedef unsigned short  elf32_hlf_t;
typedef unsigned int    elf32_off_t;
typedef unsigned int    elf32_swd_t;
typedef unsigned int    elf32_wrd_t;

#define ELFCLASS32 1
#define ELFCLASS64 2

#define PT_LOAD 1

typedef unsigned long ptr_t;

struct elf_generic_ehdr
{
    union {
        struct {
            unsigned int signature;
            unsigned char class;
        } __attribute__((packed));
        unsigned char e_ident[16];
    };
    unsigned short e_type;
    unsigned short e_machine;
    unsigned int e_version;
};

struct elf32_ehdr
{
    struct elf_generic_ehdr head;
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

struct elf64_ehdr
{
    struct elf_generic_ehdr head;
    elf64_ptr_t e_entry;
    elf64_off_t e_phoff;
    elf64_off_t e_shoff;
    elf64_wrd_t e_flags;
    elf64_hlf_t e_ehsize;
    elf64_hlf_t e_phentsize;
    elf64_hlf_t e_phnum;
    elf64_hlf_t e_shentsize;
    elf64_hlf_t e_shnum;
    elf64_hlf_t e_shstrndx;
};

struct elf64_phdr
{
    elf64_wrd_t p_type;
    elf64_wrd_t p_flags;
    elf64_off_t p_offset;
    elf64_ptr_t p_va;
    elf64_ptr_t p_pa;
    elf64_xwrd_t p_filesz;
    elf64_xwrd_t p_memsz;
    elf64_xwrd_t p_align;
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

struct elf_section
{
    ptr_t va;
    ptr_t pa;
    unsigned int flags;
    unsigned int memsz;
};

struct ksec_genctx
{
    struct elf_section* secs;
    int size;
    const char* prefix;
};

#define MAPPED_SIZE     (256 << 10)

static struct elf_generic_ehdr*
__load_elf(const char* path)
{
    int fd;
    struct elf_generic_ehdr* ehdr;
    
    fd = open(path, O_RDONLY);
    if (fd == -1) {
        printf("fail to open elf: %s\n", strerror(errno));
        return NULL;
    }

    ehdr = mmap(NULL, MAPPED_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if ((void*)ehdr == (void*)-1) {
        printf("fail to mmap elf (%d): %s\n", errno, strerror(errno));
        return NULL;
    }

    return ehdr;
}

static void
__wr_mapentry(struct ksec_genctx* ctx, struct elf_section* sec)
{
    printf("/* --- entry --- */\n");
    printf("%s 0x%lx\n", ctx->prefix, sec->va);
    printf("%s 0x%lx\n", ctx->prefix, sec->pa);
    printf(".4byte 0x%x\n", sec->memsz);
    printf(".4byte 0x%x\n", sec->flags);
}

static void
__wr_maplast(struct ksec_genctx* ctx, struct elf_section* sec)
{
    printf("/* --- entry --- */\n");
    printf("%s 0x%lx\n", ctx->prefix, sec->va);
    printf("%s 0x%lx\n", ctx->prefix, sec->pa);
    printf(".4byte (__kexec_end - 0x%lx)\n", sec->va);
    printf(".4byte 0x%x\n", sec->flags);
}

#define SIZEPF32    ".4byte"
#define SIZEPF64    ".8byte"
#define gen_ksec_map(bits, ctx, ehdr)                                           \
    ({                                                                          \
        struct elf##bits##_ehdr *_e;                                            \
        struct elf##bits##_phdr *phdr, *phent;                                  \
        _e   = (struct elf##bits##_ehdr*)(ehdr);                                \
        phdr = (struct elf##bits##_phdr*)((ptr_t)_e + _e->e_phoff);             \
        for (int i = 0, j = 0; i < _e->e_phnum; i++) {                          \
            phent = &phdr[i];                                                   \
            if (phent->p_type != PT_LOAD) {                                     \
                continue;                                                       \
            }                                                                   \
            ctx.secs[j++] = (struct elf_section) {                              \
                .va = phent->p_va,                                              \
                .pa = phent->p_pa,                                              \
                .memsz = phent->p_memsz,                                        \
                .flags = phent->p_flags,                                        \
            };                                                                  \
        }                                                                       \
    })

#define count_loadable(bits, ehdr)                                              \
    ({                                                                          \
        struct elf##bits##_ehdr *_e;                                            \
        struct elf##bits##_phdr *phdr, *phent;                                  \
        int all_loadable = 0;                                                   \
        _e   = (struct elf##bits##_ehdr*)(ehdr);                                \
        phdr = (struct elf##bits##_phdr*)((ptr_t)_e + _e->e_phoff);             \
        for (int i = 0; i < _e->e_phnum; i++) {                                 \
            phent = &phdr[i];                                                   \
            if (phent->p_type == PT_LOAD) {                                     \
                all_loadable++;                                                 \
            }                                                                   \
        }                                                                       \
        all_loadable;                                                           \
    })

static void
__generate_kernelmap(struct elf_generic_ehdr* ehdr)
{
    
    printf(".section .autogen.ksecmap, \"a\", @progbits\n"
           ".global __autogen_ksecmap\n"
           "__autogen_ksecmap:\n");

    struct ksec_genctx genctx;

    if (ehdr->class == ELFCLASS32) {
        genctx.size = count_loadable(32, ehdr);
        genctx.prefix = SIZEPF32;
    } else {
        genctx.size = count_loadable(64, ehdr);
        genctx.prefix = SIZEPF64;
    }

    genctx.secs = calloc(genctx.size, sizeof(struct elf_section));

    if (ehdr->class == ELFCLASS32) {
        gen_ksec_map(32, genctx, ehdr);
    }
    else {
        genctx.size = count_loadable(64, ehdr);
        gen_ksec_map(64, genctx, ehdr);
    }

    int i = 0;
    struct elf_section* sec_ent;

    printf(".4byte 0x%x\n", genctx.size);
    
    /*
        By convention, the last two LOAD is reserved for 
        .autogen and .bss.*.
        
        Their size will not be known until after relink,
        so we need to emit a special entry and let linker
        determine the size. (see __wr_maplast)
     */

    for (; i < genctx.size - 1; i++)
    {
        sec_ent = &genctx.secs[i];
        __wr_mapentry(&genctx, sec_ent);
    }
    
    __wr_maplast(&genctx, &genctx.secs[i]);
}

#define MODE_GETARCH 1
#define MODE_GENLOAD 2
#define MODE_ERROR   3

int 
main(int argc, char* const* argv)
{
    int c, mode;
    char *path, *out;

    path = NULL;
    mode = MODE_GETARCH;

    while ((c = getopt(argc, argv, "i:tph")) != -1)
    {
        switch (c)
        {
            case 'i':
                path = optarg;
            break;

            case 't':
                mode = MODE_GETARCH;
            break;

            case 'p':
                mode = MODE_GENLOAD;
            break;

            case 'h':
                printf("usage: elftool -i elf_file -pt [-o out]\n");
                printf("       -t: get elf type.\n");
                printf("       -p: generate load sections.\n");
                exit(1);
            break;

            default:
                printf("unknown option: '%c'", optopt);
                exit(1);
            break;
        }
    }

    if (!path) {
        printf("must specify an elf.\n");
        exit(1);
    }

    struct elf_generic_ehdr* ehdr;
    ehdr = __load_elf(path);

    if (!ehdr) {
        return 1;
    }

    if (ehdr->signature != 0x464c457fU) {
        printf("not an elf file\n");
        return 1;
    }

    if (mode == MODE_GETARCH) {
        if (ehdr->class == ELFCLASS32) {
            printf("ELF32\n");
        }
        else {
            printf("ELF64\n");
        }
        return 0;
    }

    if (mode == MODE_GENLOAD) {
        __generate_kernelmap(ehdr);
        return 0;
    }

    return 0;
}