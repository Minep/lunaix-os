#include <lunaix/mm/mmap.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>

#include <lunaix/syscall.h>
#include <lunaix/syscall_utils.h>

// any size beyond this is bullshit
#define BS_SIZE (KERNEL_MM_BASE - UMMAP_START)

int
mem_has_overlap(vm_regions_t* regions, ptr_t start, size_t len)
{
    ptr_t end = start + end - 1;
    struct mm_region *pos, *n;
    llist_for_each(pos, n, regions, head)
    {
        if (pos->end >= start && pos->start < start) {
            return 1;
        }

        if (pos->end <= end && pos->start >= start) {
            return 1;
        }

        if (pos->end >= end && pos->start < end) {
            return 1;
        }
    }

    return 0;
}

int
mem_map(void** addr_out,
        struct mm_region** created,
        void* addr,
        struct v_file* file,
        struct mmap_param* param)
{
    assert_msg(addr, "addr can not be NULL");

    ptr_t last_end = USER_START, found_loc = (ptr_t)addr;
    struct mm_region *pos, *n;

    vm_regions_t* vm_regions = &param->pvms->regions;

    if ((param->flags & MAP_FIXED_NOREPLACE)) {
        if (mem_has_overlap(vm_regions, found_loc, param->mlen)) {
            return EEXIST;
        }
        goto found;
    }

    if ((param->flags & MAP_FIXED)) {
        int status =
          mem_unmap(param->vms_mnt, vm_regions, found_loc, param->mlen);
        if (status) {
            return status;
        }
        goto found;
    }

    llist_for_each(pos, n, vm_regions, head)
    {
        if (last_end < found_loc) {
            size_t avail_space = pos->start - found_loc;
            if (pos->start > found_loc && avail_space > param->mlen) {
                goto found;
            }
            found_loc = pos->end + PG_SIZE;
        }

        last_end = pos->end;
    }

    return ENOMEM;

found:
    if (found_loc >= KERNEL_MM_BASE || found_loc < USER_START) {
        return ENOMEM;
    }

    struct mm_region* region = region_create_range(
      found_loc,
      param->mlen,
      ((param->proct | param->flags) & 0x3f) | (param->type & ~0xffff));

    region->mfile = file;
    region->foff = param->offset;
    region->flen = param->flen;
    region->proc_vms = param->pvms;

    region_add(vm_regions, region);

    u32_t attr = PG_ALLOW_USER;
    if ((param->proct & REGION_WRITE)) {
        attr |= PG_WRITE;
    }

    for (u32_t i = 0; i < param->mlen; i += PG_SIZE) {
        vmm_set_mapping(param->vms_mnt, found_loc + i, 0, attr, 0);
    }

    if (file) {
        vfs_ref_file(file);
    }

    if (addr_out) {
        *addr_out = found_loc;
    }
    if (created) {
        *created = region;
    }
    return 0;
}

void
mem_sync_pages(ptr_t mnt,
               struct mm_region* region,
               ptr_t start,
               ptr_t length,
               int options)
{
    if (!region->mfile || !(region->attr & REGION_WSHARED)) {
        return;
    }

    v_mapping mapping;
    for (size_t i = 0; i < length; i += PG_SIZE) {
        if (!vmm_lookupat(mnt, start + i, &mapping)) {
            continue;
        }

        if (PG_IS_DIRTY(*mapping.pte)) {
            size_t offset = mapping.va - region->start + region->foff;
            struct v_inode* inode = region->mfile->inode;
            region->mfile->ops->write_page(inode, mapping.va, PG_SIZE, offset);
            *mapping.pte &= ~PG_DIRTY;
            cpu_invplg(mapping.pte);
        } else if ((options & MS_INVALIDATE)) {
            goto invalidate;
        }

        if (options & MS_INVALIDATE_ALL) {
            goto invalidate;
        }

        continue;

    invalidate:
        *mapping.pte &= ~PG_PRESENT;
        pmm_free_page(KERNEL_PID, mapping.pa);
        cpu_invplg(mapping.pte);
    }
}

int
mem_msync(ptr_t mnt,
          vm_regions_t* regions,
          ptr_t addr,
          size_t length,
          int options)
{
    struct mm_region* pos = list_entry(regions->next, struct mm_region, head);
    while (length && (ptr_t)&pos->head != (ptr_t)regions) {
        if (pos->end >= addr && pos->start <= addr) {
            size_t l = MIN(length, pos->end - addr);
            mem_sync_pages(mnt, pos, addr, l, options);

            addr += l;
            length -= l;
        }
        pos = list_entry(pos->head.next, struct mm_region, head);
    }

    if (length) {
        return ENOMEM;
    }

    return 0;
}

void
mem_unmap_region(ptr_t mnt, struct mm_region* region)
{
    size_t len = ROUNDUP(region->end - region->start, PG_SIZE);
    mem_sync_pages(mnt, region, region->start, len, 0);

    for (size_t i = region->start; i <= region->end; i += PG_SIZE) {
        ptr_t pa = vmm_del_mapping(mnt, i);
        if (pa) {
            pmm_free_page(__current->pid, pa);
        }
    }
    llist_delete(&region->head);
    region_release(region);
}

int
mem_unmap(ptr_t mnt, vm_regions_t* regions, void* addr, size_t length)
{
    length = ROUNDUP(length, PG_SIZE);
    ptr_t cur_addr = PG_ALIGN(addr);
    struct mm_region *pos, *n;

    llist_for_each(pos, n, regions, head)
    {
        if (pos->start <= cur_addr && pos->end >= cur_addr) {
            break;
        }
    }

    while (&pos->head != regions && cur_addr >= pos->start) {
        u32_t l = pos->end - cur_addr;
        pos->end = cur_addr;

        if (l > length) {
            // unmap cause discontinunity in a memory region -  do split
            struct mm_region* region = valloc(sizeof(struct mm_region));
            *region = *pos;
            region->start = cur_addr + length;
            llist_insert_after(&pos->head, &region->head);
            l = length;
        }

        mem_sync_pages(mnt, pos, cur_addr, l, 0);

        for (size_t i = 0; i < l; i += PG_SIZE) {
            ptr_t pa = vmm_del_mapping(mnt, cur_addr + i);
            if (pa) {
                pmm_free_page(pos->proc_vms->pid, pa);
            }
        }

        n = container_of(pos->head.next, typeof(*pos), head);
        if (pos->end == pos->start) {
            llist_delete(&pos->head);
            region_release(pos);
        }

        pos = n;
        length -= l;
        cur_addr += length;
    }

    return 0;
}

__DEFINE_LXSYSCALL3(void*, sys_mmap, void*, addr, size_t, length, va_list, lst)
{
    int proct = va_arg(lst, int);
    int fd = va_arg(lst, u32_t);
    off_t offset = va_arg(lst, off_t);
    int options = va_arg(lst, int);
    int errno = 0;
    void* result = (void*)-1;

    if (!length || length > BS_SIZE || !PG_ALIGNED(addr)) {
        errno = EINVAL;
        goto done;
    }

    if (!addr) {
        addr = UMMAP_START;
    } else if (addr < UMMAP_START || addr + length >= UMMAP_END) {
        errno = ENOMEM;
        goto done;
    }

    struct v_fd* vfd;
    if ((errno = vfs_getfd(fd, &vfd))) {
        goto done;
    }

    struct v_file* file = vfd->file;

    if (!(options & MAP_ANON)) {
        if (!file->ops->read_page) {
            errno = ENODEV;
            goto done;
        }
    } else {
        file = NULL;
    }

    struct mmap_param param = { .flags = options,
                                .mlen = ROUNDUP(length, PG_SIZE),
                                .offset = offset,
                                .type = REGION_TYPE_GENERAL,
                                .proct = proct,
                                .pvms = &__current->mm,
                                .vms_mnt = VMS_SELF };

    errno = mem_map(&result, NULL, addr, file, &param);

done:
    __current->k_status = errno;
    return result;
}

__DEFINE_LXSYSCALL2(void, munmap, void*, addr, size_t, length)
{
    return mem_unmap(VMS_SELF, &__current->mm.regions, addr, length);
}

__DEFINE_LXSYSCALL3(int, msync, void*, addr, size_t, length, int, flags)
{
    if (!PG_ALIGNED(addr) || ((flags & MS_ASYNC) && (flags & MS_SYNC))) {
        return DO_STATUS(EINVAL);
    }

    int status =
      mem_msync(VMS_SELF, &__current->mm.regions, addr, length, flags);

    return DO_STATUS(status);
}