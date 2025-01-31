#ifndef __LUNAIX_TASKFS_H
#define __LUNAIX_TASKFS_H

#include <lunaix/fs/api.h>
#include <lunaix/fs/twimap.h>
#include <lunaix/ds/list.h>

struct task_attribute
{
    struct list_node siblings;
    struct hlist_node attrs;
    struct hstr key;
    struct twimap_ops ops;
};

#define taskfs_export_attr(name)                            \
    ({                                                      \
        taskfs_export_attr_mapping(stringify(name),         \
        (struct twimap_ops) {                               \
            __task_read_##name,                             \
            __twimap_default_gonext, __twimap_default_reset \
        });                                                 \
    })

#define taskfs_export_list_attr(name)                       \
    ({                                                      \
        taskfs_export_attr_mapping(stringify(name),         \
        (struct twimap_ops) {                               \
            __task_read_##name,                             \
            __task_gonext_##name, __task_reset_##name       \
        });                                                 \
    })

void
taskfs_init();

void
taskfs_export_attr_mapping(const char* key, struct twimap_ops ops);

struct task_attribute*
taskfs_get_attr(struct hstr* key);

void
taskfs_invalidate(pid_t pid);

#endif /* __LUNAIX_TASKFS_H */
