#ifndef _LUNAIX_UHDR_SYS_MMAN_H
#define _LUNAIX_UHDR_SYS_MMAN_H

// POSIX compliant.

// identity mapped to region attributes
#define PROT_READ (1 << 2)
#define PROT_WRITE (1 << 3)
#define PROT_EXEC (1 << 4)
#define PROT_NONE 0

// identity mapped to region attributes

#define MAP_WSHARED 0x2
#define MAP_RSHARED 0x1
#define MAP_SHARED MAP_WSHARED
#define MAP_PRIVATE MAP_RSHARED
#define MAP_EXCLUSIVE 0x0
#define MAP_ANON (1 << 5)
#define MAP_ANONYMOUS MAP_ANON
#define MAP_STACK 0 // no effect in Lunaix

// other MAP_* goes should beyond 0x20

#define MAP_FIXED 0x40
#define MAP_FIXED_NOREPLACE 0x80

#define MS_ASYNC 0x1
#define MS_SYNC 0x2
#define MS_INVALIDATE 0x4
#define MS_INVALIDATE_ALL 0x8

struct usr_mmap_param
{
    void* addr;
    unsigned long length;
    int proct;
    int flags;
    int fd;
    unsigned long offset;
};

#endif /* _LUNAIX_UHDR_MMAN_H */
