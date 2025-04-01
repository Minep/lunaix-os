#ifndef __LUNAIX_TWIFS_H
#define __LUNAIX_TWIFS_H

#include <lunaix/ds/ldga.h>
#include <lunaix/fs.h>
#include <lunaix/fs/twimap.h>
#include <lunaix/spike.h>

struct twifs_ops
{
    int (*write)(struct v_inode* inode,
                    void* buffer,
                    size_t len,
                    size_t fpos);
    int (*read)(struct v_inode* inode,
                void* buffer,
                size_t len,
                size_t fpos);
};

struct twifs_node
{
    struct hstr name;
    inode_t ino_id;
    void* data;
    u32_t itype;
    int acl;

    char name_val[VFS_NAME_MAXLEN];
    struct llist_header children;
    struct llist_header siblings;
    struct twifs_ops ops;
};

struct twifs_export
{
    char* name;
    int acl;
    struct twifs_ops ops;
};

struct twimap_export
{
    char* name;
    int acl;
    struct twimap_ops ops;
};

#define twinode_getdata(inode, type)                                           \
    ({                                                                         \
        struct twifs_node* twinode = (struct twifs_node*)(inode)->data;        \
        assert(twinode);                                                       \
        (type) twinode->data;                                                  \
    })

#define EXPORT_TWIFS_PLUGIN(label, plg_init)                                   \
    export_ldga_el(twiplugin_inits, label, ptr_t, plg_init)

#define __twifs_export_base(name_, acl_) \
            .name=stringify(name_), .acl=(acl_)

#define twifs_node_ro(name_, acl_)                        \
        struct twifs_export __twifs_exp_##name_ =           \
            { __twifs_export_base(name_, acl_),             \
                .ops = { .read =  __twifs_read_##name_ }}

#define twifs_node_rw(name_, acl_)                        \
        struct twifs_export __twifs_exp_##name_ =           \
            { __twifs_export_base(name_, acl_),             \
                .ops = { .read =  __twifs_read_##name_,         \
                         .write =  __twifs_write_##name_ }}

#define twimap_value_export(name_, acl_)                    \
        struct twimap_export __twimap_exp_##name_ =          \
            { __twifs_export_base(name_, acl_),             \
                .ops = { .read =  __twimap_read_##name_ }}

#define twimap_list_export(name_, acl_)                     \
        struct twimap_export __twimap_exp_##name_ =          \
            { __twifs_export_base(name_, acl_),             \
                .ops = { .read =  __twimap_read_##name_,         \
                         .go_next =  __twimap_gonext_##name_,   \
                         .reset =  __twimap_reset_##name_, }}

#define twifs_export(parent, name_, data_)      \
        twifs_basic_node_from(parent, &__twifs_exp_##name_, data_)

#define twimap_export(parent, name_, data_)      \
        twifs_mapped_node_from(parent, &__twimap_exp_##name_, data_)

#define twifs_export_ro(parent, name_, acl_, data_)                     \
        ({                                                              \
            twifs_node_ro(name_, acl_);                                 \
            twifs_export(parent, name_, data_);                        \
        })

#define twimap_export_value(parent, name_, acl_, data_)                 \
        ({                                                              \
            twimap_value_export(name_, acl_);                           \
            twimap_export(parent, name_, data_);                        \
        })

#define twimap_export_list(parent, name_, acl_, data_)                  \
        ({                                                              \
            twimap_list_export(name_, acl_);                            \
            twimap_export(parent, name_, data_);                        \
        })

void
twifs_register_plugins();

struct twifs_node*
twifs_file_node_vargs(struct twifs_node* parent, const char* fmt, va_list args);

struct twifs_node*
twifs_dir_node(struct twifs_node* parent, const char* fmt, ...);

int
twifs_rm_node(struct twifs_node* node);

struct twifs_node*
twifs_basic_node_from(struct twifs_node* parent, 
                        struct twifs_export* exp_def, void* data);

struct twifs_node*
twifs_mapped_node_from(struct twifs_node* parent, 
                        struct twimap_export* exp_def, void* data);

struct twimap*
twifs_mapping(struct twifs_node* parent, 
                void* data, int acl, const char* fmt, ...);

#endif /* __LUNAIX_TWIFS_H */
