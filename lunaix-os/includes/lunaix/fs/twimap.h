#ifndef __LUNAIX_TWIMAP_H
#define __LUNAIX_TWIMAP_H

#include <lunaix/types.h>

#define twimap_index(twimap, type) ((type)__ptr((twimap)->index))
#define twimap_data(twimap, type) ((type)__ptr((twimap)->data))

extern struct v_file_ops twimap_file_ops;

struct twimap;
struct twimap_ops
{
    void (*read)(struct twimap* mapping);
    int (*go_next)(struct twimap* mapping);
    void (*reset)(struct twimap* mapping);
};

struct twimap
{
    void* index;
    void* buffer;
    void* data;
    size_t size_acc;

    union {
        struct {
            void (*read)(struct twimap* mapping);
            int (*go_next)(struct twimap* mapping);
            void (*reset)(struct twimap* mapping);
        };
        struct twimap_ops ops;
    };
};

void
__twimap_default_reset(struct twimap* map);

int
__twimap_default_gonext(struct twimap* map);

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
