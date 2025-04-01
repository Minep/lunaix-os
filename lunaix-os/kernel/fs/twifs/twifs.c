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
#include <klibc/strfmt.h>
#include <klibc/string.h>
#include <lunaix/clock.h>
#include <lunaix/fs/api.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/fs/twimap.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

#include <asm/pagetable.h>

static struct twifs_node* fs_root;
static struct cake_pile* twi_pile;
static volatile u32_t inode_id = 0;

extern const struct v_file_ops twifs_file_ops;
extern const struct v_inode_ops twifs_inode_ops;

static struct twifs_node*
__twifs_new_node(struct twifs_node* parent,
                 const char* name, int name_len, u32_t itype)
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

static void
__twifs_init_inode(struct v_superblock* vsb, struct v_inode* inode)
{
    inode->ops = (struct v_inode_ops*)&twifs_inode_ops;
    inode->default_fops = (struct v_file_ops*)&twifs_file_ops;


    // we set default access right to be 0660.
    // TODO need a way to allow this to be changed
    
    fsapi_inode_setaccess(inode, FSACL_DEFAULT);
    fsapi_inode_setowner(inode, 0, 0);
}

static int
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

static int
__twifs_unmount(struct v_superblock* vsb)
{
    return 0;
}

static int
__twifs_fwrite(struct v_inode* inode, void* buffer, size_t len, size_t fpos)
{
    struct twifs_node* twi_node = (struct twifs_node*)inode->data;
    if (!twi_node || !twi_node->ops.write) {
        return ENOTSUP;
    }
    return twi_node->ops.write(inode, buffer, len, fpos);
}

static int
__twifs_fwrite_pg(struct v_inode* inode, void* buffer, size_t fpos)
{
    return __twifs_fwrite(inode, buffer, PAGE_SIZE, fpos);
}

static int
__twifs_fread(struct v_inode* inode, void* buffer, size_t len, size_t fpos)
{
    struct twifs_node* twi_node = (struct twifs_node*)inode->data;
    if (!twi_node || !twi_node->ops.read) {
        return ENOTSUP;
    }
    return twi_node->ops.read(inode, buffer, len, fpos);
}

static int
__twifs_fread_pg(struct v_inode* inode, void* buffer, size_t fpos)
{
    return __twifs_fread(inode, buffer, PAGE_SIZE, fpos);
}

static struct twifs_node*
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

static int
__twifs_dirlookup(struct v_inode* inode, struct v_dnode* dnode)
{
    struct twifs_node* twi_node = (struct twifs_node*)inode->data;

    if (!check_directory_node(inode)) {
        return ENOTDIR;
    }

    struct twifs_node* twinode = __twifs_get_node(twi_node, &dnode->name);
    if (twinode) {
        struct v_inode* child_inode = vfs_i_find(inode->sb, twinode->ino_id);
        if (child_inode) {
            goto done;
        }

        if (!(child_inode = vfs_i_alloc(inode->sb))) {
            return ENOENT;
        }

        child_inode->id = twinode->ino_id;
        child_inode->itype = twinode->itype;
        child_inode->data = twinode;

        if (twinode->acl != -1) {
            fsapi_inode_setaccess(child_inode, twinode->acl);
        }

        vfs_i_addhash(child_inode);

    done:
        dnode->data = twinode->data;
        vfs_assign_inode(dnode, child_inode);
        return 0;
    }
    return ENOENT;
}

static int
__twifs_iterate_dir(struct v_file* file, struct dir_context* dctx)
{
    struct twifs_node* twi_node = (struct twifs_node*)(file->inode->data);
    unsigned int counter = 2;
    struct twifs_node *pos, *n;

    if (fsapi_handle_pseudo_dirent(file, dctx)) {
        return 1;
    }

    llist_for_each(pos, n, &twi_node->children, siblings)
    {
        if (counter++ >= file->f_pos) {
            fsapi_dir_report(
              dctx, pos->name.value, pos->name.len, vfs_get_dtype(pos->itype));
            return 1;
        }
    }

    return 0;
}

static int
__twifs_openfile(struct v_inode* inode, struct v_file* file)
{
    struct twifs_node* twi_node = (struct twifs_node*)inode->data;
    if (twi_node) {
        return 0;
    }
    return ENOTSUP;
}

static inline struct twifs_node*
__twifs_create_node(struct twifs_node* parent,
                    const char* name, int acl)
{
    struct twifs_node* twi_node;
    unsigned int size;

    size = strlen(name);
    parent = parent ?: fs_root;
    twi_node = __twifs_new_node(parent, name, size, F_FILE);
    twi_node->acl = acl;

    return twi_node;
}

static int
__twifs_twimap_file_read(struct v_inode* inode, 
                         void* buf, size_t len, size_t fpos)
{
    struct twimap* map = twinode_getdata(inode, struct twimap*);
    return twimap_read(map, buf, len, fpos);
}


int
twifs_rm_node(struct twifs_node* node)
{
    if (check_itype(node->itype, VFS_IFDIR) && !llist_empty(&node->children)) {
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
    size_t len = ksnprintfv(buf, fmt, VFS_NAME_MAXLEN, args);

    return __twifs_new_node(parent ? parent : fs_root, buf, len, F_FILE);
}

struct twifs_node*
twifs_dir_node(struct twifs_node* parent, const char* fmt, ...)
{
    size_t len;
    char buf[VFS_NAME_MAXLEN];
    struct twifs_node* twi_node;

    va_list args;
    va_start(args, fmt);

    len = ksnprintfv(buf, fmt, VFS_NAME_MAXLEN, args);
    twi_node =
      __twifs_new_node(parent ? parent : fs_root, buf, len, VFS_IFDIR);

    twi_node->acl = FSACL_aR | FSACL_aX;

    va_end(args);

    return twi_node;
}


struct twifs_node*
twifs_basic_node_from(struct twifs_node* parent, 
                        struct twifs_export* def, void* data)
{
    struct twifs_node* twi_node;
    
    twi_node = __twifs_create_node(parent, def->name, def->acl);
    twi_node->ops = def->ops;
    twi_node->data = data;

    return twi_node;
}

struct twifs_node*
twifs_mapped_node_from(struct twifs_node* parent, 
                        struct twimap_export* def, void* data)
{
    struct twifs_node* twi_node;
    struct twimap* map_;
    
    map_     = twimap_create(data);
    twi_node = __twifs_create_node(parent, def->name, def->acl);

    twi_node->data = map_;
    twi_node->ops.read = __twifs_twimap_file_read;
    // twi_node->ops.write = __twifs_twimap_file_write;

    if (def->ops.go_next) {
        map_->ops.go_next = def->ops.go_next;
    }

    if (def->ops.reset) {
        map_->ops.reset = def->ops.reset;
    }

    map_->ops.read = def->ops.read;

    return twi_node;
}

struct twimap*
twifs_mapping(struct twifs_node* parent, 
                void* data, int acl, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    struct twimap* map = twimap_create(data);
    struct twifs_node* node = twifs_file_node_vargs(parent, fmt, args);
    
    node->ops.read = __twifs_twimap_file_read;
    node->data = map;
    node->acl = acl;

    return map;
}

void
twifs_register_plugins()
{
    ldga_invoke_fn0(twiplugin_inits);
}

const struct v_file_ops twifs_file_ops = { .close = default_file_close,
                                           .read = __twifs_fread,
                                           .read_page = __twifs_fread_pg,
                                           .write = __twifs_fwrite,
                                           .write_page = __twifs_fwrite_pg,
                                           .readdir = __twifs_iterate_dir };

const struct v_inode_ops twifs_inode_ops = { .dir_lookup = __twifs_dirlookup,
                                             .mkdir = default_inode_mkdir,
                                             .rmdir = default_inode_rmdir,
                                             .open = __twifs_openfile };


static void
twifs_init()
{
    struct filesystem* fs;
    fs = fsapi_fs_declare("twifs", FSTYPE_PSEUDO | FSTYPE_ROFS);
    
    fsapi_fs_set_mntops(fs, __twifs_mount, __twifs_unmount);
    fsapi_fs_finalise(fs);

    twi_pile = cake_new_pile("twifs_node", sizeof(struct twifs_node), 1, 0);
    fs_root = twifs_dir_node(NULL, NULL, 0, 0);
}
EXPORT_FILE_SYSTEM(twifs, twifs_init);