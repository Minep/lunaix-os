#include <lunaix/fs.h>
#include <lunaix/fs/twimap.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

#include <klibc/strfmt.h>
#include <klibc/string.h>

#include <sys/mm/pagetable.h>

#define TWIMAP_BUFFER_SIZE PAGE_SIZE

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

static int
__twimap_file_read(struct v_inode* inode, void* buf, size_t len, size_t fpos)
{
    struct twimap* map = (struct twimap*)(inode->data);
    return twimap_read(map, buf, len, fpos);
}

static int
__twimap_file_read_page(struct v_inode* inode, void* buf, size_t fpos)
{
    return __twimap_file_read(inode, buf, PAGE_SIZE, fpos);
}

int
twimap_read(struct twimap* map, void* buffer, size_t len, size_t fpos)
{
    map->buffer = valloc(TWIMAP_BUFFER_SIZE);
    map->size_acc = 0;

    map->reset(map);

    // FIXME what if TWIMAP_BUFFER_SIZE is not big enough?

    size_t pos = map->size_acc;
    while (pos <= fpos) {
        map->size_acc = 0;
        map->read(map);
        pos += map->size_acc;
        
        if (!map->go_next(map)) {
            break;
        }
    }

    if (pos <= fpos) {
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

    if (acc_size <= len - 1) {
        // pad zero
        *(char*)(buffer + acc_size + 1) = 0;
    }

    vfree(map->buffer);
    return acc_size;
}

void
twimap_printf(struct twimap* mapping, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char* buf = mapping->buffer + mapping->size_acc;

    mapping->size_acc += ksnprintfv(buf, fmt, TWIMAP_BUFFER_SIZE, args) - 1;

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

struct twimap*
twimap_create(void* data)
{
    struct twimap* map = vzalloc(sizeof(struct twimap));
    map->reset = __twimap_default_reset;
    map->go_next = __twimap_default_gonext;
    map->data = data;

    return map;
}

struct v_file_ops twimap_file_ops = { .close = default_file_close,
                                      .read = __twimap_file_read,
                                      .read_page = __twimap_file_read_page,
                                      .readdir = default_file_readdir,
                                      .seek = default_file_seek,
                                      .write = default_file_write };