#include <lunaix/mm/mmap.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>

#include <lunaix/syscall.h>
#include <lunaix/syscall_utils.h>

// any size beyond this is bullshit
#define BS_SIZE (2 << 30)

int
mem_map(void** addr_out,
        ptr_t mnt,
        vm_regions_t* regions,
        void* addr,
        struct v_file* file,
        off_t offset,
        size_t length,
        u32_t proct,
        u32_t options)
{
    ptr_t last_end = USER_START;
    struct mm_region *pos, *n;

    if ((options & MAP_FIXED)) {
        pos = region_get(regions, addr);
        if (!pos) {
            last_end = addr;
            goto found;
        }
        return EEXIST;
    }

    llist_for_each(pos, n, regions, head)
    {
        if (pos->start - last_end >= length && last_end >= addr) {
            goto found;
        }
        last_end = pos->end;
    }

    return ENOMEM;

found:
    addr = last_end;

    struct mm_region* region =
      region_create_range(addr, length, proct | (options & 0x1f));
    region->mfile = file;
    region->offset = offset;

    region_add(regions, region);

    u32_t attr = PG_ALLOW_USER;
    if ((proct & REGION_WRITE)) {
        attr |= PG_WRITE;
    }

    for (u32_t i = 0; i < length; i += PG_SIZE) {
        vmm_set_mapping(mnt, addr + i, 0, attr, 0);
    }

    *addr_out = addr;
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
            size_t offset = mapping.va - region->start + region->offset;
            struct v_inode* inode = region->mfile->inode;
            region->mfile->ops->write_page(inode, mapping.va, PG_SIZE, offset);
            *mapping.pte &= ~PG_DIRTY;
            cpu_invplg(mapping.va);
        } else if ((options & MS_INVALIDATE)) {
            *mapping.pte &= ~PG_PRESENT;
            cpu_invplg(mapping.va);
        }
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

int
mem_unmap(ptr_t mnt, vm_regions_t* regions, void* addr, size_t length)
{
    length = ROUNDUP(length, PG_SIZE);
    ptr_t cur_addr = ROUNDDOWN((ptr_t)addr, PG_SIZE);
    struct mm_region *pos, *n;

    llist_for_each(pos, n, regions, head)
    {
        if (pos->start <= cur_addr) {
            break;
        }
    }

    while (&pos->head != regions && cur_addr > pos->start) {
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
                pmm_free_page(__current->pid, pa);
            }
        }

        n = container_of(pos->head.next, typeof(*pos), head);
        if (pos->end == pos->start) {
            llist_delete(&pos->head);
            vfree(pos);
        }

        pos = n;
        length -= l;
        cur_addr += length;
    }
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

    length = ROUNDUP(length, PG_SIZE);

    errno = mem_map(&result,
                    VMS_SELF,
                    &__current->mm.regions,
                    addr,
                    file,
                    offset,
                    length,
                    proct,
                    options);

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