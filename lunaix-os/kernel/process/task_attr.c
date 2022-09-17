#include <lunaix/fs/taskfs.h>

void
__read_pending_sig(struct twimap* map)
{
    struct proc_info* proc = twimap_data(map, struct proc_info*);
    twimap_printf(map, "%bb", proc->sig_pending);
}

void
__read_masked_sig(struct twimap* map)
{
    struct proc_info* proc = twimap_data(map, struct proc_info*);
    twimap_printf(map, "%bb", proc->sig_mask);
}

void
__read_parent(struct twimap* map)
{
    struct proc_info* proc = twimap_data(map, struct proc_info*);
    twimap_printf(map, "%d", proc->parent->pid);
}

void
__read_ctimestamp(struct twimap* map)
{
    struct proc_info* proc = twimap_data(map, struct proc_info*);
    twimap_printf(map, "%d", proc->created);
}

void
__read_pgid(struct twimap* map)
{
    struct proc_info* proc = twimap_data(map, struct proc_info*);
    twimap_printf(map, "%d", proc->pgid);
}

void
__read_children(struct twimap* map)
{
    struct llist_header* proc_list = twimap_index(map, struct llist_header*);
    struct proc_info* proc =
      container_of(proc_list, struct proc_info, siblings);
    if (!proc)
        return;
    twimap_printf(map, "%d ", proc->pid);
}

int
__next_children(struct twimap* map)
{
    struct llist_header* proc = twimap_index(map, struct llist_header*);
    struct proc_info* current = twimap_data(map, struct proc_info*);
    if (!proc)
        return 0;
    map->index = proc->next;
    if (map->index == &current->children) {
        return 0;
    }
    return 1;
}

void
__reset_children(struct twimap* map)
{
    struct proc_info* proc = twimap_data(map, struct proc_info*);
    if (llist_empty(&proc->children)) {
        map->index = 0;
        return;
    }
    map->index = proc->children.next;
}

void
export_task_attr()
{
    struct twimap* map;
    map = twimap_create(NULL);
    map->read = __read_pending_sig;
    taskfs_export_attr("sig_pending", map);

    map = twimap_create(NULL);
    map->read = __read_masked_sig;
    taskfs_export_attr("sig_masked", map);

    map = twimap_create(NULL);
    map->read = __read_parent;
    taskfs_export_attr("parent", map);

    map = twimap_create(NULL);
    map->read = __read_ctimestamp;
    taskfs_export_attr("created", map);

    map = twimap_create(NULL);
    map->read = __read_pgid;
    taskfs_export_attr("pgid", map);

    map = twimap_create(NULL);
    map->read = __read_children;
    map->go_next = __next_children;
    map->reset = __reset_children;
    taskfs_export_attr("children", map);
}