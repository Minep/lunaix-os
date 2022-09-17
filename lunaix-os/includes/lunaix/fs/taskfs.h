#ifndef __LUNAIX_TASKFS_H
#define __LUNAIX_TASKFS_H

#include <lunaix/fs.h>
#include <lunaix/fs/twimap.h>

struct task_attribute
{
    struct llist_header siblings;
    struct hlist_node attrs;
    struct hstr key;
    struct twimap* map_file;
    char key_val[32];
};

void
taskfs_init();

void
taskfs_export_attr(const char* key, struct twimap* attr_map_file);

struct task_attribute*
taskfs_get_attr(struct hstr* key);

void
taskfs_invalidate(pid_t pid);

#endif /* __LUNAIX_TASKFS_H */
