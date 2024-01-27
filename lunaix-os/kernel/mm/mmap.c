#include <lunaix/mm/mmap.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>
#include <lunaix/syscall.h>
#include <lunaix/syscall_utils.h>

#include <sys/mm/mempart.h>

#include <usr/lunaix/mann_flags.h>

// any size beyond this is bullshit
#define BS_SIZE (KERNEL_EXEC - USR_MMAP)

int
mem_has_overlap(vm_regions_t* regions, ptr_t start, ptr_t end)
{
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
mem_adjust_inplace(vm_regions_t* regions,
                   struct mm_region* region,
                   ptr_t newend)
{
    ssize_t len = newend - region->start;
    if (len == 0) {
        return 0;
    }

    if (len < 0) {
        return EINVAL;
    }

    if (mem_has_overlap(regions, region->start, newend)) {
        return ENOMEM;
    }

    region->end = newend;

    return 0;
}

int
mem_map(void** addr_out,
        struct mm_region** created,
        ptr_t addr,
        struct v_file* file,
        struct mmap_param* param)
{
    assert_msg(addr, "addr can not be NULL");

    ptr_t last_end = USR_EXEC, found_loc = addr;
    struct mm_region *pos, *n;

    vm_regions_t* vm_regions = &param->pvms->regions;

    if ((param->flags & MAP_FIXED_NOREPLACE)) {
        if (mem_has_overlap(vm_regions, found_loc, param->mlen + found_loc)) {
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
            found_loc = pos->end + MEM_PAGE;
        }

        last_end = pos->end;
    }

    return ENOMEM;

found:
    if (found_loc >= KERNEL_EXEC || found_loc < USR_EXEC) {
        return ENOMEM;
    }

    struct mm_region* region = region_create_range(
      found_loc,
      param->mlen,
      ((param->proct | param->flags) & 0x3f) | (param->type & ~0xffff));

    region->mfile = file;
    region->foff = param->offset;
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
        *addr_out = (void*)found_loc;
    }
    if (created) {
        *created = region;
    }
    return 0;
}

int
mem_remap(void** addr_out,
          struct mm_region** remapped,
          void* addr,
          struct v_file* file,
          struct mmap_param* param)
{
    // TODO

    return EINVAL;
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

            region->mfile->ops->write_page(inode, (void*)mapping.va, offset);

            *mapping.pte &= ~PG_DIRTY;

            cpu_flush_page((ptr_t)mapping.pte);
        } else if ((options & MS_INVALIDATE)) {
            goto invalidate;
        }

        if (options & MS_INVALIDATE_ALL) {
            goto invalidate;
        }

        continue;

    invalidate:
        *mapping.pte &= ~PG_PRESENT;
        pmm_free_page(mapping.pa);
        cpu_flush_page((ptr_t)mapping.pte);
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
            pmm_free_page(pa);
        }
    }
    llist_delete(&region->head);
    region_release(region);
}

// Case: head inseted, tail inseted
#define CASE_HITI(vmr, addr, len)                                              \
    ((vmr)->start <= (addr) && ((addr) + (len)) <= (vmr)->end)

// Case: head inseted, tail extruded
#define CASE_HITE(vmr, addr, len)                                              \
    ((vmr)->start <= (addr) && ((addr) + (len)) > (vmr)->end)

// Case: head extruded, tail inseted
#define CASE_HETI(vmr, addr, len)                                              \
    ((vmr)->start > (addr) && ((addr) + (len)) <= (vmr)->end)

// Case: head extruded, tail extruded
#define CASE_HETE(vmr, addr, len)                                              \
    ((vmr)->start > (addr) && ((addr) + (len)) > (vmr)->end)

static void
__unmap_overlapped_cases(ptr_t mnt,
                         struct mm_region* vmr,
                         ptr_t* addr,
                         size_t* length)
{
    // seg start, umapped segement start
    ptr_t seg_start = *addr, umps_start = 0;

    // seg len, umapped segement len
    size_t seg_len = *length, umps_len = 0;

    size_t displ = 0, shrink = 0;

    if (CASE_HITI(vmr, seg_start, seg_len)) {
        size_t new_start = seg_start + seg_len;

        // Require a split
        if (new_start < vmr->end) {
            struct mm_region* region = region_dup(vmr);
            if (region->mfile) {
                size_t f_shifted = new_start - region->start;
                region->foff += f_shifted;
            }
            region->start = new_start;
            llist_insert_after(&vmr->head, &region->head);
        }

        shrink = vmr->end - seg_start;
        umps_len = shrink;
        umps_start = seg_start;
    } else if (CASE_HITE(vmr, seg_start, seg_len)) {
        shrink = vmr->end - seg_start;
        umps_len = shrink;
        umps_start = seg_start;
    } else if (CASE_HETI(vmr, seg_start, seg_len)) {
        displ = seg_len - (vmr->start - seg_start);
        umps_len = displ;
        umps_start = vmr->start;
    } else if (CASE_HETE(vmr, seg_start, seg_len)) {
        shrink = vmr->end - vmr->start;
        umps_len = shrink;
        umps_start = vmr->start;
    }

    mem_sync_pages(mnt, vmr, vmr->start, umps_len, 0);
    for (size_t i = 0; i < umps_len; i += PG_SIZE) {
        ptr_t pa = vmm_del_mapping(mnt, vmr->start + i);
        if (pa) {
            pmm_free_page(pa);
        }
    }

    vmr->start += displ;
    vmr->end -= shrink;

    if (vmr->start >= vmr->end) {
        llist_delete(&vmr->head);
        region_release(vmr);
    } else if (vmr->mfile) {
        vmr->foff += displ;
    }

    *addr = umps_start + umps_len;

    size_t ump_len = *addr - seg_start;
    *length = MAX(seg_len, ump_len) - ump_len;
}

int
mem_unmap(ptr_t mnt, vm_regions_t* regions, ptr_t addr, size_t length)
{
    length = ROUNDUP(length, PG_SIZE);
    ptr_t cur_addr = PG_ALIGN(addr);
    struct mm_region *pos, *n;

    llist_for_each(pos, n, regions, head)
    {
        u32_t l = pos->start - cur_addr;
        if ((pos->start <= cur_addr && cur_addr < pos->end) || l <= length) {
            break;
        }
    }

    while (&pos->head != regions && length) {
        n = container_of(pos->head.next, typeof(*pos), head);
        __unmap_overlapped_cases(mnt, pos, &cur_addr, &length);

        pos = n;
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

    ptr_t addr_ptr = (ptr_t)addr;

    if (!length || length > BS_SIZE || !PG_ALIGNED(addr_ptr)) {
        errno = EINVAL;
        goto done;
    }

    if (!addr_ptr) {
        addr_ptr = USR_MMAP;
    } else if (addr_ptr < USR_MMAP || addr_ptr + length >= USR_MMAP_END) {
        if (!(options & (MAP_FIXED | MAP_FIXED_NOREPLACE))) {
            errno = ENOMEM;
            goto done;
        }
    }

    struct v_file* file = NULL;

    if (!(options & MAP_ANON)) {
        struct v_fd* vfd;
        if ((errno = vfs_getfd(fd, &vfd))) {
            goto done;
        }

        file = vfd->file;
        if (!file->ops->read_page) {
            errno = ENODEV;
            goto done;
        }
    }

    struct mmap_param param = { .flags = options,
                                .mlen = ROUNDUP(length, PG_SIZE),
                                .flen = length,
                                .offset = offset,
                                .type = REGION_TYPE_GENERAL,
                                .proct = proct,
                                .pvms = vmspace(__current),
                                .vms_mnt = VMS_SELF };

    errno = mem_map(&result, NULL, addr_ptr, file, &param);

done:
    __current->k_status = errno;
    return result;
}

__DEFINE_LXSYSCALL2(int, munmap, void*, addr, size_t, length)
{
    return mem_unmap(
      VMS_SELF, vmregions(__current), (ptr_t)addr, length);
}

__DEFINE_LXSYSCALL3(int, msync, void*, addr, size_t, length, int, flags)
{
    if (!PG_ALIGNED(addr) || ((flags & MS_ASYNC) && (flags & MS_SYNC))) {
        return DO_STATUS(EINVAL);
    }

    int status = mem_msync(VMS_SELF,
                           vmregions(__current),
                           (ptr_t)addr,
                           length,
                           flags);

    return DO_STATUS(status);
}