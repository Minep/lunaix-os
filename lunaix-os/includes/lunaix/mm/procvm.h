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
    ptr_t             vm_mnt;       // current mount point
    vm_regions_t      regions;

    struct mm_region* heap;
    struct proc_info* proc;
    struct proc_mm*   guest_mm;     // vmspace mounted by this vmspace
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
procvm_dupvms_mount(struct proc_mm* proc);

void
procvm_unmount_release(struct proc_mm* proc);

void
procvm_mount(struct proc_mm* mm);

void
procvm_unmount(struct proc_mm* mm);

/**
 * @brief Initialize and mount the vms of `proc` as a clean slate which contains
 * nothing but shared global mapping of kernel image.
 * 
 * @param proc 
 */
void
procvm_initvms_mount(struct proc_mm* mm);


/*
    Mount and unmount from VMS_SELF.
    Although every vms is mounted to that spot by default,
    this just serve the purpose to ensure the scheduled
    vms does not dangling in some other's vms.
*/

void
procvm_mount_self(struct proc_mm* mm);

void
procvm_unmount_self(struct proc_mm* mm);


/*
    remote virtual memory manipulation
*/

#define REMOTEVM_MAX_PAGES 128

ptr_t
procvm_enter_remote_transaction(struct remote_vmctx* rvmctx, struct proc_mm* mm,
                    ptr_t remote_base, size_t size);

int
procvm_copy_remote(struct remote_vmctx* rvmctx, 
                   ptr_t remote_dest, void* local_src, size_t sz);

void
procvm_exit_remote(struct remote_vmctx* rvmctx);

/*
    architecture-specific
*/

void
procvm_link_kernel(ptr_t dest_mnt);

void
procvm_unlink_kernel();

#endif /* __LUNAIX_PROCVM_H */
