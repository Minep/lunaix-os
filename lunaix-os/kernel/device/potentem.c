#include <lunaix/device.h>
#include <lunaix/mm/valloc.h>

struct potens_meta*
alloc_potens(int cap, unsigned int size) 
{
    struct potens_meta* cm = (struct potens_meta*)vzalloc(size);

    cm->pot_type = cap;

    return cm;
}

void
device_grant_potens(struct device* dev, struct potens_meta* cap)
{
    llist_append(&dev->potentium, &cap->potentes);
    cap->owner = dev;
}

struct potens_meta*
device_get_potens(struct device* dev, unsigned int pot_type)
{
    struct potens_meta *pos, *n;

    llist_for_each(pos, n, &dev->potentium, potentes) {
        if (pos->pot_type == pot_type){
            return pos;
        }
    }

    return NULL;
}