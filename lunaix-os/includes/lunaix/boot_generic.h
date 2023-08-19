#ifndef __LUNAIX_BOOT_GENERIC_H
#define __LUNAIX_BOOT_GENERIC_H

#include <lunaix/types.h>

// Free memory region
#define BOOT_MMAP_FREE 0

// Reserved memory region
#define BOOT_MMAP_RSVD 1

// Reclaimable memory region
#define BOOT_MMAP_RCLM 2

struct boot_mmapent
{
    ptr_t start;
    size_t size;
    int type;
};

struct boot_modent
{
    ptr_t start;
    ptr_t end;
    char* str;
};

struct boot_handoff
{
    size_t msize;
    struct
    {
        size_t size;
        struct boot_mmapent* mmap;
        size_t mmap_len;
    } mem;

    struct
    {
        ptr_t ksections;
        size_t size;

        char** argv;
        size_t argc;
    } kexec;

    struct
    {
        size_t mods_num;
        struct boot_modent* entries;
    } mods;

    void (*release)(struct boot_handoff*);
    void (*prepare)(struct boot_handoff*);

    // XXX: should arch specific boot detect platform interface provider?
};

#ifndef __BOOT_CODE__
void
boot_begin(struct boot_handoff*);

void
boot_end(struct boot_handoff*);

void
boot_cleanup();
#endif

#endif /* __LUNAIX_BOOT_GENERIC_H */
