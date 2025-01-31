#include <lunaix/fs/taskfs.h>
#include <lunaix/process.h>

#define proc(map)   (twimap_data(map, struct proc_info*))

void
__task_read_parent(struct twimap* map)
{
    twimap_printf(map, "%d", proc(map)->parent->pid);
}

void
__task_read_created(struct twimap* map)
{
    twimap_printf(map, "%d", proc(map)->created);
}

void
__task_read_pgid(struct twimap* map)
{
    twimap_printf(map, "%d", proc(map)->pgid);
}

void
__task_read_cmdline(struct twimap* map)
{
    twimap_printf(map, "%s", proc(map)->cmd);
}

static inline void
__get_protection(struct mm_region* vmr, char* prot_buf)
{
    prot_buf[0] = (vmr->attr & REGION_READ) ? 'r' : '-';
    prot_buf[1] = (vmr->attr & REGION_WRITE) ? 'w' : '-';
    prot_buf[2] = (vmr->attr & REGION_EXEC) ? 'x' : '-';
    prot_buf[3] = shared_writable_region(vmr) ? 's' : 'p';
    prot_buf[4] = 0;
}

static inline void
__get_vmr_name(struct mm_region* vmr, char* buf, unsigned int size)
{
    int region_type;

    region_type = (vmr->attr >> 16) & 0xf;

    if (region_type == REGION_TYPE_STACK) {
        strcpy(buf, "[stack]");
    }

    else if (region_type == REGION_TYPE_HEAP) {
        strcpy(buf, "[heap]");
    }

    else if (vmr->mfile) {
        vfs_get_path(vmr->mfile->dnode, buf, size, 0);
    }
    
    else {
        buf[0] = 0;
    }
}

void
__task_read_maps(struct twimap* map)
{
    struct llist_header* vmr_;
    struct mm_region* vmr;
    unsigned int size;

    vmr_ = twimap_index(map, struct llist_header*);
    vmr = container_of(vmr_, struct mm_region, head);
    
    assert(vmr_);

    char prot[5], name[256];
    
    __get_protection(vmr, prot);
    __get_vmr_name(vmr, name, 256);

    size = vmr->end - vmr->start;

    twimap_printf(map, "%012lx-%012lx %x %s    %s\n", 
                 vmr->start, vmr->end, size, prot, name);
}

int
__task_gonext_maps(struct twimap* map)
{
    struct proc_mm* mm;
    struct llist_header* vmr;

    vmr = twimap_index(map, struct llist_header*);
    mm = vmspace(proc(map));

    if (!vmr)
        return 0;
        
    map->index = vmr->next;
    if (map->index == &mm->regions) {
        return 0;
    }
    
    return 1;
}

void
__task_reset_maps(struct twimap* map)
{
    struct proc_mm* mm;

    mm = vmspace(proc(map));
    if (llist_empty(&mm->regions)) {
        map->index = 0;
        return;
    }

    map->index = mm->regions.next;
}

void
export_task_attr()
{
    taskfs_export_attr(parent);
    taskfs_export_attr(created);
    taskfs_export_attr(pgid);
    taskfs_export_attr(cmdline);
    taskfs_export_list_attr(maps);
}