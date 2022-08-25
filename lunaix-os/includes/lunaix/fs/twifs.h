#ifndef __LUNAIX_TWIFS_H
#define __LUNAIX_TWIFS_H

#include <lunaix/fs.h>

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

void
twifs_init();

struct twifs_node*
twifs_file_node(struct twifs_node* parent, const char* fmt, ...);

struct twifs_node*
twifs_dir_node(struct twifs_node* parent, const char* fmt, ...);

int
twifs_rm_node(struct twifs_node* node);

#endif /* __LUNAIX_TWIFS_H */
