#include <lunaix/mm/mmap.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>
#include <lunaix/syscall.h>
#include <lunaix/syscall_utils.h>

#include <sys/mm/mm_defs.h>

#include <usr/lunaix/mann_flags.h>

// any size beyond this is bullshit
#define BS_SIZE (KERNEL_RESIDENT - USR_MMAP)

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
mmap_user(void** addr_out,
        struct mm_region** created,
        ptr_t addr,
        struct v_file* file,
        struct mmap_param* param) 
{
    param->range_end = KERNEL_RESIDENT;
    param->range_start = USR_EXEC;

    return mem_map(addr_out, created, addr, file, param);
}

static void
__remove_ranged_mappings(pte_t* ptep, size_t npages)
{
    struct leaflet* leaflet;
    pte_t pte; 
    for (size_t i = 0, n = 0; i < npages; i++, ptep++) {
        pte = pte_at(ptep);

        set_pte(ptep, null_pte);
        if (!pte_isloaded(pte)) {
            continue;
        }

        leaflet = pte_leaflet_aligned(pte);
        leaflet_return(leaflet);

        n = ptep_unmap_leaflet(ptep, leaflet) - 1;
        i += n;
        ptep += n;
    }
}

static ptr_t
__mem_find_slot_backward(struct mm_region* lead, struct mmap_param* param, struct mm_region* anchor)
{
    ptr_t size = param->mlen;
    struct mm_region *pos = anchor, 
                     *n = next_region(pos);
    while (pos != lead)
    {
        if (pos == lead) {
            break;
        }

        ptr_t end = n->start;
        if (n == lead) {
            end = param->range_end;
        }

        if (end - pos->end >= size) {
            return pos->end;
        }

        pos = n;
        n = next_region(pos);
    }
    
    return 0;
}

static ptr_t
__mem_find_slot_forward(struct mm_region* lead, struct mmap_param* param, struct mm_region* anchor)
{
    ptr_t size = param->mlen;
    struct mm_region *pos = anchor, 
                     *prev = prev_region(pos);
    while (lead != pos)
    {
        ptr_t end = prev->end;
        if (prev == lead) {
            end = param->range_start;
        }

        if (pos->start - end >= size) {
            return pos->start - size;
        }

        pos = prev;
        prev = prev_region(pos);
    }

    return 0;
}

static ptr_t
__mem_find_slot(vm_regions_t* lead, struct mmap_param* param, struct mm_region* anchor)
{
    ptr_t result = 0;
    struct mm_region* _lead = get_region(lead);
    if ((result = __mem_find_slot_backward(_lead, param, anchor))) {
        return result;
    }

    return __mem_find_slot_forward(_lead, param, anchor);
}

static struct mm_region*
__mem_find_nearest(vm_regions_t* lead, ptr_t addr)
{   
    ptr_t min_dist = (ptr_t)-1;
    struct mm_region *pos, *n, *min = NULL;
    llist_for_each(pos, n, lead, head) {
        if (region_contains(pos, addr)) {
            return pos;
        }

        ptr_t dist = addr - pos->end;
        if (addr < pos->start) {
            dist = pos->start - addr;
        }

        if (dist < min_dist) {
            min_dist = dist;
            min = pos;
        }
    }

    return min;
}

int
mem_map(void** addr_out,
        struct mm_region** created,
        ptr_t addr,
        struct v_file* file,
        struct mmap_param* param)
{
    assert_msg(addr, "addr can not be NULL");

    ptr_t last_end = USR_EXEC, found_loc = page_aligned(addr);
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

    if (llist_empty(vm_regions)) {
        goto found;
    }

    struct mm_region* anchor = __mem_find_nearest(vm_regions, found_loc);
    if ((found_loc = __mem_find_slot(vm_regions, param, anchor))) {
        goto found;
    }

    return ENOMEM;

found:
    if (found_loc >= param->range_end || found_loc < param->range_start) {
        return ENOMEM;
    }

    struct mm_region* region = region_create_range(
      found_loc,
      param->mlen,
      ((param->proct | param->flags) & 0x3f) | (param->type & ~0xffff));

    region->mfile = file;
    region->flen = param->flen;
    region->foff = param->offset;
    region->proc_vms = param->pvms;

    region_add(vm_regions, region);
    
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
    
    pte_t* ptep = mkptep_va(mnt, start);
    ptr_t va    = page_aligned(start);

    for (; va < start + length; va += PAGE_SIZE, ptep++) {
        pte_t pte = vmm_tryptep(ptep, LFT_SIZE);
        if (pte_isnull(pte)) {
            continue;
        }

        if (pte_dirty(pte)) {
            size_t offset = va - region->start + region->foff;
            struct v_inode* inode = region->mfile->inode;

            region->mfile->ops->write_page(inode, (void*)va, offset);

            set_pte(ptep, pte_mkclean(pte));
            tlb_flush_vmr(region, va);
            
        } else if ((options & MS_INVALIDATE)) {
            goto invalidate;
        }

        if (options & MS_INVALIDATE_ALL) {
            goto invalidate;
        }

        continue;

        // FIXME what if mem_sync range does not aligned with
        //       a leaflet with order > 1
    invalidate:
        set_pte(ptep, null_pte);
        leaflet_return(pte_leaflet(pte));
        tlb_flush_vmr(region, va);
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
    if (!region) {
        return;
    }
    
    valloc_ensure_valid(region);
    
    pfn_t pglen = leaf_count(region->end - region->start);
    mem_sync_pages(mnt, region, region->start, pglen * PAGE_SIZE, 0);

    pte_t* ptep = mkptep_va(mnt, region->start);
    __remove_ranged_mappings(ptep, pglen);

    tlb_flush_vmr_all(region);
    
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
    } 
    else if (CASE_HITE(vmr, seg_start, seg_len)) {
        shrink = vmr->end - seg_start;
        umps_len = shrink;
        umps_start = seg_start;
    } 
    else if (CASE_HETI(vmr, seg_start, seg_len)) {
        displ = seg_len - (vmr->start - seg_start);
        umps_len = displ;
        umps_start = vmr->start;
    } 
    else if (CASE_HETE(vmr, seg_start, seg_len)) {
        shrink = vmr->end - vmr->start;
        umps_len = shrink;
        umps_start = vmr->start;
    }

    mem_sync_pages(mnt, vmr, vmr->start, umps_len, 0);

    pte_t *ptep = mkptep_va(mnt, vmr->start);
    __remove_ranged_mappings(ptep, leaf_count(umps_len));

    tlb_flush_vmr_range(vmr, vmr->start, umps_len);

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
    length = ROUNDUP(length, PAGE_SIZE);
    ptr_t cur_addr = page_aligned(addr);
    struct mm_region *pos, *n;

    llist_for_each(pos, n, regions, head)
    {
        u32_t l = pos->start - cur_addr;
        if ((pos->start <= cur_addr && cur_addr < pos->end) || l <= length) {
            break;
        }
    }

    size_t remaining = length;
    while (&pos->head != regions && remaining) {
        n = container_of(pos->head.next, typeof(*pos), head);
        if (pos->start > cur_addr + length) {
            break;
        }

        __unmap_overlapped_cases(mnt, pos, &cur_addr, &remaining);

        pos = n;
    }

    return 0;
}

__DEFINE_LXSYSCALL1(void*, sys_mmap, struct usr_mmap_param*, mparam)
{
    off_t offset;
    size_t length;
    int proct, fd, options;
    int errno;
    void* result;
    ptr_t addr_ptr;

    proct = mparam->proct;
    fd = mparam->fd;
    offset = mparam->offset;
    options = mparam->flags;
    addr_ptr = __ptr(mparam->addr);
    length = mparam->length;

    errno  = 0;
    result = (void*)-1;

    if (!length || length > BS_SIZE || va_offset(addr_ptr)) {
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

    length = ROUNDUP(length, PAGE_SIZE);
    struct mmap_param param = { .flags = options,
                                .mlen = length,
                                .flen = length,
                                .offset = offset,
                                .type = REGION_TYPE_GENERAL,
                                .proct = proct,
                                .pvms = vmspace(__current),
                                .vms_mnt = VMS_SELF };

    errno = mmap_user(&result, NULL, addr_ptr, file, &param);

done:
    syscall_result(errno);
    return result;
}

__DEFINE_LXSYSCALL2(int, munmap, void*, addr, size_t, length)
{
    return mem_unmap(
      VMS_SELF, vmregions(__current), (ptr_t)addr, length);
}

__DEFINE_LXSYSCALL3(int, msync, void*, addr, size_t, length, int, flags)
{
    if (va_offset((ptr_t)addr) || ((flags & MS_ASYNC) && (flags & MS_SYNC))) {
        return DO_STATUS(EINVAL);
    }

    int status = mem_msync(VMS_SELF,
                           vmregions(__current),
                           (ptr_t)addr,
                           length,
                           flags);

    return DO_STATUS(status);
}