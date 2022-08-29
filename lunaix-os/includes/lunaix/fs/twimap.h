#ifndef __LUNAIX_TWIMAP_H
#define __LUNAIX_TWIMAP_H

#include <lunaix/types.h>

#define twimap_index(twimap, type) ((type)((twimap)->index))
#define twimap_data(twimap, type) ((type)((twimap)->data))

extern struct v_file_ops twimap_file_ops;

struct twimap
{
    void* index;
    void* buffer;
    void* data;
    size_t size_acc;
    void (*read)(struct twimap* mapping);
    int (*go_next)(struct twimap* mapping);
    void (*reset)(struct twimap* mapping);
};

int
twimap_read(struct twimap* map, void* buffer, size_t len, size_t fpos);

void
twimap_printf(struct twimap* mapping, const char* fmt, ...);

int
twimap_memcpy(struct twimap* mapping, const void* src, const size_t len);

int
twimap_memappend(struct twimap* mapping, const void* src, const size_t len);

struct twimap*
twimap_create(void* data);

#endif /* __LUNAIX_TWIMAP_H */
