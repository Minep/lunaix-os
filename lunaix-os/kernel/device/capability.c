#include <lunaix/device.h>
#include <lunaix/mm/valloc.h>

struct capability_meta*
alloc_capability(int cap, unsigned int size) 
{
    struct capability_meta* cm = (struct capability_meta*)vzalloc(size);

    cm->cap_type = cap;

    return cm;
}

void
device_grant_capability(struct device* dev, struct capability_meta* cap)
{
    llist_append(&dev->capabilities, &cap->caps);
}

struct capability_meta*
device_get_capability(struct device* dev, unsigned int cap_type)
{
    struct capability_meta *pos, *n;

    llist_for_each(pos, n, &dev->capabilities, caps) {
        if (pos->cap_type == cap_type){
            return pos;
        }
    }

    return NULL;
}