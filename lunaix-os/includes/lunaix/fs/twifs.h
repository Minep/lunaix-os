#ifndef __LUNAIX_TWIFS_H
#define __LUNAIX_TWIFS_H

#include <lunaix/fs.h>

struct twifs_node
{
    struct v_inode* inode;
    struct hstr name;
    void* data;
    uint32_t itype;
    struct llist_header children;
    struct llist_header siblings;
    struct
    {
        int (*write)(struct v_file* file,
                     void* buffer,
                     size_t len,
                     size_t fpos);
        int (*read)(struct v_file* file, void* buffer, size_t len, size_t fpos);
    } ops;
};

void
twifs_init();

struct twifs_node*
twifs_file_node(struct twifs_node* parent,
                const char* name,
                int name_len,
                uint32_t itype);

struct twifs_node*
twifs_dir_node(struct twifs_node* parent,
               const char* name,
               int name_len,
               uint32_t itype);

struct twifs_node*
twifs_toplevel_node(const char* name, int name_len, uint32_t itype);

int
twifs_rm_node(struct twifs_node* node);

#endif /* __LUNAIX_TWIFS_H */
