#ifndef __LUNAIX_TWIFS_H
#define __LUNAIX_TWIFS_H

#include <lunaix/fs.h>
#include <lunaix/spike.h>

struct twifs_node
{
    struct hstr name;
    inode_t ino_id;
    void* data;
    uint32_t itype;
    char name_val[VFS_NAME_MAXLEN];
    struct llist_header children;
    struct llist_header siblings;
    struct
    {
        int (*write)(struct v_inode* inode,
                     void* buffer,
                     size_t len,
                     size_t fpos);
        int (*read)(struct v_inode* inode,
                    void* buffer,
                    size_t len,
                    size_t fpos);
    } ops;
};

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

#define twinode_getdata(inode, type)                                           \
    ({                                                                         \
        struct twifs_node* twinode = (struct twifs_node*)(inode)->data;        \
        assert(twinode);                                                       \
        (type) twinode->data;                                                  \
    })

void
twifs_init();

struct twifs_node*
twifs_file_node_vargs(struct twifs_node* parent, const char* fmt, va_list args);

struct twifs_node*
twifs_file_node(struct twifs_node* parent, const char* fmt, ...);

struct twifs_node*
twifs_dir_node(struct twifs_node* parent, const char* fmt, ...);

int
twifs_rm_node(struct twifs_node* node);

#define twimap_index(twimap, type) ((type)((twimap)->index))
#define twimap_data(twimap, type) ((type)((twimap)->data))

struct twimap*
twifs_mapping(struct twifs_node* parent, void* data, const char* fmt, ...);

void
twimap_printf(struct twimap* mapping, const char* fmt, ...);

int
twimap_memcpy(struct twimap* mapping, const void* src, const size_t len);

int
twimap_memappend(struct twimap* mapping, const void* src, const size_t len);

#endif /* __LUNAIX_TWIFS_H */
