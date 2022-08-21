#ifndef __LUNAIX_DEVICE_H
#define __LUNAIX_DEVICE_H

#define DEVICE_NAME_SIZE 32

#include <lunaix/ds/hstr.h>
#include <lunaix/ds/llist.h>

#define DEV_MSKIF 0x00000003

#define DEV_IFVOL 0x0 // volumetric (block) device
#define DEV_IFSEQ 0x1 // sequential (character) device
#define DEV_IFCAT 0x2 // a device category (as device groupping)

typedef unsigned int dev_t;

struct device
{
    struct llist_header siblings;
    struct device* parent;
    struct hstr name;
    dev_t dev_id;
    int dev_type;
    char name_val[DEVICE_NAME_SIZE];
    void* underlay;
    int (*read)(struct device* dev, void* buf, size_t offset, size_t len);
    int (*write)(struct device* dev, void* buf, size_t offset, size_t len);
};

struct device*
device_addseq(struct device* parent, void* underlay, char* name_fmt, ...);

struct device*
device_addvol(struct device* parent, void* underlay, char* name_fmt, ...);

struct device*
device_addcat(struct device* parent, char* name_fmt, ...);

void
device_remove(struct device* dev);

struct device*
device_getbyid(struct llist_header* devlist, dev_t id);

struct device*
device_getbyname(struct llist_header* devlist, struct hstr* name);

struct device*
device_getbyoffset(struct llist_header* devlist, int pos);

#endif /* __LUNAIX_DEVICE_H */
