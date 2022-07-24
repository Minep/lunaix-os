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

static struct twifs_node fs_root;

static struct cake_pile* twi_pile;

int
__twifs_dirlookup(struct v_inode* inode, struct v_dnode* dnode);

int
__twifs_openfile(struct v_inode* inode, struct v_file* file);

struct twifs_node*
__twifs_get_node(struct twifs_node* parent, struct hstr* name);

void
twifs_init()
{
    twi_pile = cake_new_pile("twifs_node", sizeof(struct twifs_node), 1, 0);

    struct filesystem* twifs = vzalloc(sizeof(struct filesystem));
    twifs->fs_name = HSTR("twifs", 5);
    twifs->mount = __twifs_mount;

    fsm_register(twifs);

    memset(&fs_root, 0, sizeof(fs_root));
    llist_init_head(&fs_root.children);

    // 预备一些常用的类别
    twifs_toplevel_node("kernel", 6);
    twifs_toplevel_node("dev", 3);
    twifs_toplevel_node("bus", 3);
}

struct twifs_node*
twifs_child_node(struct twifs_node* parent, const char* name, int name_len)
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
    llist_append(&parent->children, &node->siblings);

    return node;
}

struct twifs_node*
twifs_toplevel_node(const char* name, int name_len)
{
    return twifs_child_node(&fs_root, name, name_len);
}

void
__twifs_mount(struct v_superblock* vsb, struct v_dnode* mount_point)
{
    mount_point->inode = __twifs_create_inode(&fs_root);
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
}

struct twifs_node*
__twifs_get_node(struct twifs_node* parent, struct hstr* name)
{
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
        dnode->inode = __twifs_create_inode(child_node);
        return 0;
    }
    return VFS_ENODIR;
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
            dctx->read_complete_callback(dctx, pos->name.value, pos->itype);
            return 0;
        }
    }

    return VFS_EENDOFDIR;
}

int
__twifs_openfile(struct v_inode* inode, struct v_file* file)
{
    struct twifs_node* twi_node = (struct twifs_node*)inode->data;
    if (twi_node) {
        file->ops = twi_node->fops;
        return 1;
    }
    return 0;
}
