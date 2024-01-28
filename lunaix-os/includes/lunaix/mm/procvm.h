#ifndef __LUNAIX_PROCVM_H
#define __LUNAIX_PROCVM_H

#include <lunaix/ds/llist.h>
#include <lunaix/ds/mutex.h>
#include <lunaix/fs.h>
#include <lunaix/types.h>

struct proc_mm;
struct proc_info;

struct mm_region
{
    struct llist_header head; // must be first field!
    struct proc_mm* proc_vms;

    // file mapped to this region
    struct v_file* mfile;
    // mapped file offset
    off_t foff;
    // mapped file length
    u32_t flen; // XXX it seems that we don't need this actually..

    ptr_t start;
    ptr_t end;
    u32_t attr;

    void** index; // fast reference, to accelerate access to this very region.

    void* data;
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
    // virtual memory root (i.e. root page table)
    ptr_t vmroot;
    vm_regions_t regions;
    struct mm_region* heap;
    struct mm_region* stack;
    struct proc_info* proc;
};

struct proc_mm*
procvm_create(struct proc_info* proc);

void
procvm_dup(struct proc_info* proc);

void
procvm_cleanup(ptr_t vm_mnt, struct proc_info* proc);


#endif /* __LUNAIX_PROCVM_H */
