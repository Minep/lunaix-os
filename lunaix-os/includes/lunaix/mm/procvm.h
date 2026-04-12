#ifndef __LUNAIX_PROCVM_H
#define __LUNAIX_PROCVM_H

#include <lunaix/ds/llist.h>
#include <lunaix/ds/mutex.h>
#include <lunaix/fs.h>
#include <lunaix/types.h>
#include <lunaix/mm/mm.h>

struct proc_mm;
struct proc_info;

struct remote_vmctx
{
    ptr_t vms_mnt;
    ptr_t local_mnt;
    ptr_t remote;
    pfn_t page_cnt;
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
    ptr_t             vmroot;
    vm_regions_t      regions;

    struct mm_region* heap;
    struct proc_info* proc;
};

/**
 * @brief Create a process virtual memory space descriptor
 * 
 * @param proc 
 * @return struct proc_mm* 
 */
struct proc_mm*
procvm_create(struct proc_info* proc);

/**
 * @brief Initialize and mount the vm of `proc` to duplication of current process
 * 
 * @param proc 
 * @return struct proc_mm* 
 */
void
procvm_dupvms(struct proc_mm* proc);

void
procvm_unmount_release(struct proc_mm* proc);

/**
 * @brief Initialize and mount the vms of `proc` as a clean slate which contains
 * nothing but shared global mapping of kernel image.
 * 
 * @param proc 
 */
void
procvm_initvms_mount(struct proc_mm* mm);

/*
    architecture-specific
*/

void
procvm_link_kernel(pte_t* dest_l0t, pte_t* src_l0t);

void
procvm_unlink_kernel();

#endif /* __LUNAIX_PROCVM_H */
