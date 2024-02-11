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
    size_t page_cnt;
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
 * @brief Initialize the vm of `proc` to duplication of current process
 * 
 * @param proc 
 * @return struct proc_mm* 
 */
void
procvm_dup(struct proc_info* proc);

void
procvm_cleanup(ptr_t vm_mnt, struct proc_info* proc);


/**
 * @brief Initialize the vm of `proc` as a clean slate which contains
 * nothing but shared global mapping of kernel image.
 * 
 * @param proc 
 */
void
procvm_init_clean(struct proc_info* proc);


/*
    remote virtual memory manipulation
*/

#define REMOTEVM_MAX_PAGES 128

ptr_t
procvm_enter_remote_transaction(struct remote_vmctx* rvmctx, struct proc_mm* mm,
                    ptr_t vm_mnt, ptr_t remote_base, size_t size);

int
procvm_copy_remote(struct remote_vmctx* rvmctx, 
                   ptr_t remote_dest, void* local_src, size_t sz);

void
procvm_exit_remote_transaction(struct remote_vmctx* rvmctx);

#endif /* __LUNAIX_PROCVM_H */
