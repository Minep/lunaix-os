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
#include <klibc/stdio.h>
#include <klibc/string.h>
#include <lunaix/clock.h>
#include <lunaix/fs.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/fs/twimap.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

static struct twifs_node* fs_root;

static struct cake_pile* twi_pile;

static volatile u32_t inode_id = 0;

extern const struct v_file_ops twifs_file_ops;
extern const struct v_inode_ops twifs_inode_ops;

struct twifs_node*
__twifs_new_node(struct twifs_node* parent,
                 const char* name,
                 int name_len,
                 u32_t itype)
{
    struct twifs_node* node = cake_grab(twi_pile);
    memset(node, 0, sizeof(*node));

    strncpy(node->name_val, name, VFS_NAME_MAXLEN);

    node->name = HSTR(node->name_val, MIN(name_len, VFS_NAME_MAXLEN));
    node->itype = itype;
    node->ino_id = inode_id++;
    hstr_rehash(&node->name, HSTR_FULL_HASH);
    llist_init_head(&node->children);

    if (parent) {
        llist_append(&parent->children, &node->siblings);
    }

    return node;
}

void
__twifs_init_inode(struct v_superblock* vsb, struct v_inode* inode)
{
    inode->ops = &twifs_inode_ops;
    inode->default_fops = &twifs_file_ops;
}

int
__twifs_mount(struct v_superblock* vsb, struct v_dnode* mount_point)
{
    vsb->ops.init_inode = __twifs_init_inode;

    struct v_inode* inode = vfs_i_alloc(vsb);
    if (!inode) {
        return ENOMEM;
    }

    inode->id = fs_root->ino_id;
    inode->itype = fs_root->itype;
    inode->data = fs_root;

    vfs_i_addhash(inode);
    vfs_assign_inode(mount_point, inode);
    return 0;
}

int
__twifs_fwrite(struct v_inode* inode, void* buffer, size_t len, size_t fpos)
{
    struct twifs_node* twi_node = (struct twifs_node*)inode->data;
    if (!twi_node || !twi_node->ops.write) {
        return ENOTSUP;
    }
    return twi_node->ops.write(inode, buffer, len, fpos);
}

int
__twifs_fread(struct v_inode* inode, void* buffer, size_t len, size_t fpos)
{
    struct twifs_node* twi_node = (struct twifs_node*)inode->data;
    if (!twi_node || !twi_node->ops.read) {
        return ENOTSUP;
    }
    return twi_node->ops.read(inode, buffer, len, fpos);
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

    if (!(twi_node->itype & VFS_IFDIR)) {
        return ENOTDIR;
    }

    struct twifs_node* child_node = __twifs_get_node(twi_node, &dnode->name);
    if (child_node) {
        struct v_inode* child_inode = vfs_i_find(inode->sb, child_node->ino_id);
        if (child_inode) {
            goto done;
        }

        if (!(child_inode = vfs_i_alloc(inode->sb))) {
            return ENOENT;
        }

        child_inode->id = child_node->ino_id;
        child_inode->itype = child_node->itype;
        child_inode->data = child_node;

        vfs_i_addhash(child_inode);

    done:
        dnode->data = child_node->data;
        vfs_assign_inode(dnode, child_inode);
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
              dctx, pos->name.value, pos->name.len, vfs_get_dtype(pos->itype));
            return 1;
        }
    }

    return 0;
}

int
__twifs_openfile(struct v_inode* inode, struct v_file* file)
{
    struct twifs_node* twi_node = (struct twifs_node*)inode->data;
    if (twi_node) {
        return 0;
    }
    return ENOTSUP;
}

int
twifs_rm_node(struct twifs_node* node)
{
    if ((node->itype & VFS_IFDIR) && !llist_empty(&node->children)) {
        return ENOTEMPTY;
    }
    llist_delete(&node->siblings);
    cake_release(twi_pile, node);
    return 0;
}

struct twifs_node*
twifs_file_node_vargs(struct twifs_node* parent, const char* fmt, va_list args)
{
    char buf[VFS_NAME_MAXLEN];
    size_t len = __ksprintf_internal(buf, fmt, VFS_NAME_MAXLEN, args);

    return __twifs_new_node(parent ? parent : fs_root, buf, len, VFS_IFSEQDEV);
}

struct twifs_node*
twifs_file_node(struct twifs_node* parent, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    struct twifs_node* twi_node = twifs_file_node_vargs(parent, fmt, args);

    va_end(args);

    return twi_node;
}

struct twifs_node*
twifs_dir_node(struct twifs_node* parent, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char buf[VFS_NAME_MAXLEN];
    size_t len = __ksprintf_internal(buf, fmt, VFS_NAME_MAXLEN, args);
    struct twifs_node* twi_node =
      __twifs_new_node(parent ? parent : fs_root, buf, len, VFS_IFDIR);

    va_end(args);

    return twi_node;
}

void
twifs_init()
{
    twi_pile = cake_new_pile("twifs_node", sizeof(struct twifs_node), 1, 0);

    struct filesystem* twifs = vzalloc(sizeof(struct filesystem));
    twifs->fs_name = HSTR("twifs", 5);
    twifs->mount = __twifs_mount;
    twifs->types = FSTYPE_ROFS;
    twifs->fs_id = 0;

    fsm_register(twifs);

    fs_root = twifs_dir_node(NULL, NULL, 0, 0);
}

int
__twifs_twimap_file_read(struct v_inode* inode,
                         void* buf,
                         size_t len,
                         size_t fpos)
{
    struct twimap* map = twinode_getdata(inode, struct twimap*);
    return twimap_read(map, buf, len, fpos);
}

struct twimap*
twifs_mapping(struct twifs_node* parent, void* data, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    struct twimap* map = twimap_create(data);
    struct twifs_node* node = twifs_file_node_vargs(parent, fmt, args);
    node->ops.read = __twifs_twimap_file_read;
    node->data = map;

    return map;
}

const struct v_file_ops twifs_file_ops = { .close = default_file_close,
                                           .read = __twifs_fread,
                                           .read_page = __twifs_fread,
                                           .write = __twifs_fwrite,
                                           .write_page = __twifs_fwrite,
                                           .readdir = __twifs_iterate_dir };

const struct v_inode_ops twifs_inode_ops = { .dir_lookup = __twifs_dirlookup,
                                             .mkdir = default_inode_mkdir,
                                             .rmdir = default_inode_rmdir,
                                             .open = __twifs_openfile };