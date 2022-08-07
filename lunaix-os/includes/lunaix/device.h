#ifndef __LUNAIX_DEVICE_H
#define __LUNAIX_DEVICE_H

#define DEVICE_NAME_SIZE 32

#include <lunaix/ds/hstr.h>
#include <lunaix/ds/llist.h>

struct device
{
    struct llist_header dev_list;
    struct device* parent;
    struct hstr name;
    char name_val[DEVICE_NAME_SIZE];
    void* underlay;
    void* fs_node;
    int (*read)(struct device* dev,
                void* buf,
                unsigned int offset,
                unsigned int len);
    int (*write)(struct device* dev,
                 void* buf,
                 unsigned int offset,
                 unsigned int len);
};

void
device_init();

struct device*
device_addseq(struct device* parent, void* underlay, char* name_fmt, ...);

struct device*
device_addvol(struct device* parent, void* underlay, char* name_fmt, ...);

void
device_remove(struct device* dev);

#endif /* __LUNAIX_DEVICE_H */
