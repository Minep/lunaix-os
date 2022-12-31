#ifndef __LUNAIX_MM_H
#define __LUNAIX_MM_H

#include <lunaix/ds/llist.h>
#include <lunaix/ds/mutex.h>
#include <lunaix/fs.h>
#include <lunaix/types.h>

#include <usr/sys/mann_flags.h>

/**
 * @brief 私有区域，该区域中的页无法进行任何形式的共享。
 *
 */
#define REGION_PRIVATE MAP_EXCLUSIVE

/**
 * @brief
 * 读共享区域，该区域中的页可以被两个进程之间读共享，但任何写操作须应用Copy-On-Write
 * 等价于 POSIX 的 MAP_PRIVATE
 *
 */
#define REGION_RSHARED MAP_PRIVATE

/**
 * @brief
 * 写共享区域，该区域中的页可以被两个进程之间读共享，任何的写操作无需执行Copy-On-Write
 * 等价于 POSIX 的 MAP_SHARED
 */
#define REGION_WSHARED MAP_SHARED

#define REGION_PERM_MASK 0x1c
#define REGION_MODE_MASK 0x3

#define REGION_READ PROT_READ
#define REGION_WRITE PROT_WRITE
#define REGION_EXEC PROT_EXEC
#define REGION_ANON MAP_ANON
#define REGION_RW REGION_READ | REGION_WRITE

#define REGION_TYPE_CODE (1 << 16)
#define REGION_TYPE_GENERAL (2 << 16)
#define REGION_TYPE_HEAP (3 << 16)
#define REGION_TYPE_STACK (4 << 16)
#define REGION_TYPE_VARS (5 << 16)

struct proc_mm;

struct mm_region
{
    struct llist_header head; // must be first field!
    struct proc_mm* proc_vms;

    // file mapped to this region
    struct v_file* mfile;
    // mapped file offset
    off_t foff;
    // mapped file length
    u32_t flen;

    ptr_t start;
    ptr_t end;
    u32_t attr;

    void** index; // fast reference, to accelerate access to this very region.

    void* data;
    // When a page is mapped and required initialize
    int (*init_page)(struct mm_region*, void*, off_t);
    // when a region is copied
    void (*region_copied)(struct mm_region*);
    // when a region is unmapped
    void (*destruct_region)(struct mm_region*);
};

static inline void
mm_index(void** index, struct mm_region* target)
{
    *index = (void*)target;
    target->index = index;
}

typedef struct llist_header vm_regions_t;

struct proc_mm
{
    vm_regions_t regions;
    struct mm_region* heap;
    struct mm_region* stack;
    pid_t pid;
};

#endif /* __LUNAIX_MM_H */
