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
    struct v_file_ops fops;
};

void
twifs_init();

struct twifs_node*
twifs_file_node(struct twifs_node* parent, const char* name, int name_len);

struct twifs_node*
twifs_dir_node(struct twifs_node* parent, const char* name, int name_len);

struct twifs_node*
twifs_toplevel_node(const char* name, int name_len);

#endif /* __LUNAIX_TWIFS_H */
