#ifndef __LUNAIX_TWIFS_H
#define __LUNAIX_TWIFS_H

#include <lunaix/fs.h>
#include <lunaix/fs/twimap.h>
#include <lunaix/spike.h>

struct twifs_node
{
    struct hstr name;
    inode_t ino_id;
    void* data;
    u32_t itype;
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

struct twimap*
twifs_mapping(struct twifs_node* parent, void* data, const char* fmt, ...);

#endif /* __LUNAIX_TWIFS_H */
