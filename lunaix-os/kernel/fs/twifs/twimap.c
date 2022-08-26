#include <lunaix/fs/twifs.h>
#include <lunaix/mm/valloc.h>

#include <klibc/stdio.h>
#include <klibc/string.h>

#define TWIMAP_BUFFER_SIZE 1024

void
__twimap_default_reset(struct twimap* map)
{
    map->index = NULL;
}

int
__twimap_default_gonext(struct twimap* map)
{
    return 0;
}

int
__twimap_read(struct v_inode* inode, void* buffer, size_t len, size_t fpos)
{
    struct twimap* map = twinode_getdata(inode, struct twimap*);
    map->buffer = valloc(TWIMAP_BUFFER_SIZE);
    map->reset(map);

    // FIXME what if TWIMAP_BUFFER_SIZE is not big enough?

    size_t pos = 0;
    do {
        map->size_acc = 0;
        map->read(map);
        pos += map->size_acc;
    } while (pos < fpos && map->go_next(map));

    if (pos < fpos) {
        vfree(map->buffer);
        return 0;
    }

    if (!fpos) {
        pos = 0;
    }

    size_t acc_size = MIN(len, map->size_acc - (pos - fpos)), rdlen = acc_size;
    memcpy(buffer, map->buffer + (pos - fpos), acc_size);

    while (acc_size < len && map->go_next(map)) {
        map->size_acc = 0;
        map->read(map);
        rdlen = MIN(len - acc_size, map->size_acc);
        memcpy(buffer + acc_size, map->buffer, rdlen);
        acc_size += rdlen;
    }

    vfree(map->buffer);
    return acc_size;
}

struct twimap*
twifs_mapping(struct twifs_node* parent, void* data, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    struct twimap* map = vzalloc(sizeof(struct twimap));
    struct twifs_node* node = twifs_file_node_vargs(parent, fmt, args);
    node->ops.read = __twimap_read;
    node->data = map;

    map->reset = __twimap_default_reset;
    map->go_next = __twimap_default_gonext;
    map->data = data;

    return map;
}

void
twimap_printf(struct twimap* mapping, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char* buf = mapping->buffer + mapping->size_acc;

    mapping->size_acc +=
      __ksprintf_internal(buf, fmt, TWIMAP_BUFFER_SIZE, args);

    va_end(args);
}

int
twimap_memcpy(struct twimap* mapping, const void* src, const size_t len)
{
    mapping->size_acc = MIN(TWIMAP_BUFFER_SIZE, len);
    memcpy(mapping->buffer, src, mapping->size_acc);

    return mapping->size_acc;
}

int
twimap_memappend(struct twimap* mapping, const void* src, const size_t len)
{
    size_t cpy_len = MIN(TWIMAP_BUFFER_SIZE - mapping->size_acc, len);
    memcpy(mapping->buffer + mapping->size_acc, src, cpy_len);
    mapping->size_acc += cpy_len;

    return cpy_len;
}