/**
 * @file twifs.c
 * @author Lunaixsky (zelong56@gmail.com)
 * @brief TwiFS - A pseudo file system for kernel state exposure.
 * @version 0.1
 * @date 2022-07-21
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <klibc/string.h>
#include <lunaix/clock.h>
#include <lunaix/fs.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/valloc.h>

static struct twifs_node* fs_root;

static struct cake_pile* twi_pile;

int
__twifs_dirlookup(struct v_inode* inode, struct v_dnode* dnode);

int
__twifs_openfile(struct v_inode* inode, struct v_file* file);

struct twifs_node*
__twifs_get_node(struct twifs_node* parent, struct hstr* name);

struct v_inode*
__twifs_create_inode(struct twifs_node* twi_node);

int
__twifs_iterate_dir(struct v_file* file, struct dir_context* dctx);

int
__twifs_mount(struct v_superblock* vsb, struct v_dnode* mount_point);

void
twifs_init()
{
    twi_pile = cake_new_pile("twifs_node", sizeof(struct twifs_node), 1, 0);

    struct filesystem* twifs = vzalloc(sizeof(struct filesystem));
    twifs->fs_name = HSTR("twifs", 5);
    twifs->mount = __twifs_mount;

    fsm_register(twifs);

    fs_root = twifs_dir_node(NULL, NULL, 0);

    // 预备一些常用的类别
    twifs_toplevel_node("kernel", 6);
    twifs_toplevel_node("dev", 3);
    twifs_toplevel_node("bus", 3);
}

struct twifs_node*
__twifs_new_node(struct twifs_node* parent, const char* name, int name_len)
{
    struct hstr hname = HSTR(name, name_len);
    hstr_rehash(&hname, HSTR_FULL_HASH);

    struct twifs_node* node = __twifs_get_node(parent, &hname);
    if (node) {
        return node;
    }

    node = cake_grab(twi_pile);
    memset(node, 0, sizeof(*node));

    node->name = hname;
    llist_init_head(&node->children);

    if (parent) {
        llist_append(&parent->children, &node->siblings);
    }

    return node;
}

struct twifs_node*
twifs_file_node(struct twifs_node* parent, const char* name, int name_len)
{
    struct twifs_node* twi_node = __twifs_new_node(parent, name, name_len);
    twi_node->itype = VFS_INODE_TYPE_FILE;

    struct v_inode* twi_inode = __twifs_create_inode(twi_node);
    twi_node->inode = twi_inode;

    return twi_inode;
}

struct twifs_node*
twifs_dir_node(struct twifs_node* parent, const char* name, int name_len)
{
    struct twifs_node* twi_node = __twifs_new_node(parent, name, name_len);
    twi_node->itype = VFS_INODE_TYPE_DIR;
    twi_node->fops.readdir = __twifs_iterate_dir;

    struct v_inode* twi_inode = __twifs_create_inode(twi_node);
    struct twifs_node* dot = __twifs_new_node(twi_node, ".", 1);
    struct twifs_node* ddot = __twifs_new_node(twi_node, "..", 2);

    dot->itype = VFS_INODE_TYPE_DIR;
    ddot->itype = VFS_INODE_TYPE_DIR;

    twi_node->inode = twi_inode;
    dot->inode = twi_inode;
    ddot->inode = parent ? parent->inode : twi_inode;

    return twi_node;
}

struct twifs_node*
twifs_toplevel_node(const char* name, int name_len)
{
    return twifs_dir_node(fs_root, name, name_len);
}

int
__twifs_mount(struct v_superblock* vsb, struct v_dnode* mount_point)
{
    mount_point->inode = fs_root->inode;
    // FIXME: try to mitigate this special case or pull it up to higher level of
    // abstraction
    if (mount_point->parent && mount_point->parent->inode) {
        struct hstr ddot_name = HSTR("..", 2);
        struct twifs_node* root_ddot = __twifs_get_node(fs_root, &ddot_name);
        root_ddot->inode = mount_point->parent->inode;
    }
    return 0;
}

struct v_inode*
__twifs_create_inode(struct twifs_node* twi_node)
{
    struct v_inode* inode = vfs_i_alloc();
    *inode = (struct v_inode){ .ctime = 0,
                               .itype = twi_node->itype,
                               .lb_addr = 0,
                               .lb_usage = 0,
                               .data = twi_node,
                               .mtime = 0,
                               .ref_count = 0 };
    inode->ops.dir_lookup = __twifs_dirlookup;
    inode->ops.open = __twifs_openfile;

    return inode;
}

struct twifs_node*
__twifs_get_node(struct twifs_node* parent, struct hstr* name)
{
    if (!parent)
        return NULL;

    struct twifs_node *pos, *n;
    llist_for_each(pos, n, &parent->children, siblings)
    {
        if (HSTR_EQ(&pos->name, name)) {
            return pos;
        }
    }
    return NULL;
}

int
__twifs_dirlookup(struct v_inode* inode, struct v_dnode* dnode)
{
    struct twifs_node* twi_node = (struct twifs_node*)inode->data;

    struct twifs_node* child_node = __twifs_get_node(twi_node, &dnode->name);
    if (child_node) {
        dnode->inode = child_node->inode;
        return 0;
    }
    return ENOENT;
}

int
__twifs_iterate_dir(struct v_file* file, struct dir_context* dctx)
{
    struct twifs_node* twi_node = (struct twifs_node*)(file->inode->data);
    int counter = 0;
    struct twifs_node *pos, *n;

    llist_for_each(pos, n, &twi_node->children, siblings)
    {
        if (counter++ >= dctx->index) {
            dctx->index = counter;
            dctx->read_complete_callback(
              dctx, pos->name.value, pos->name.len, pos->itype);
            return 0;
        }
    }

    return 1;
}

int
__twifs_openfile(struct v_inode* inode, struct v_file* file)
{
    struct twifs_node* twi_node = (struct twifs_node*)inode->data;
    if (twi_node) {
        file->inode = twi_node->inode;
        file->ops = twi_node->fops;
        return 0;
    }
    return ENOTSUP;
}
