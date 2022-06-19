#ifndef __LUNAIX_MM_H
#define __LUNAIX_MM_H

#include <lunaix/ds/llist.h>
#include <lunaix/ds/mutex.h>

typedef struct
{
    void* start;
    void* brk;
    void* max_addr;
    mutex_t lock;
} heap_context_t;

/**
 * @brief 私有区域，该区域中的页无法进行任何形式的共享。
 *
 */
#define REGION_PRIVATE 0x0

/**
 * @brief
 * 读共享区域，该区域中的页可以被两个进程之间读共享，但任何写操作须应用Copy-On-Write
 *
 */
#define REGION_RSHARED 0x1

/**
 * @brief
 * 写共享区域，该区域中的页可以被两个进程之间读共享，任何的写操作无需执行Copy-On-Write
 *
 */
#define REGION_WSHARED 0x2

#define REGION_PERM_MASK 0x1c
#define REGION_MODE_MASK 0x3

#define REGION_READ (1 << 2)
#define REGION_WRITE (1 << 3)
#define REGION_EXEC (1 << 4)
#define REGION_RW REGION_READ | REGION_WRITE

#define REGION_TYPE_CODE (1 << 16);
#define REGION_TYPE_GENERAL (2 << 16);
#define REGION_TYPE_HEAP (3 << 16);
#define REGION_TYPE_STACK (4 << 16);

struct mm_region
{
    struct llist_header head;
    unsigned long start;
    unsigned long end;
    unsigned int attr;
};

#endif /* __LUNAIX_MM_H */
