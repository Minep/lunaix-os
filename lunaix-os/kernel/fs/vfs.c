#include <klibc/string.h>
#include <lunaix/fs.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/page.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

#define PATH_DELIM '/'
#define DNODE_HASHTABLE_BITS 10
#define DNODE_HASHTABLE_SIZE (1 << DNODE_HASHTABLE_BITS)
#define DNODE_HASH_MASK (DNODE_HASHTABLE_SIZE - 1)
#define DNODE_HASHBITS (32 - DNODE_HASHTABLE_BITS)

static struct cake_pile* dnode_pile;
static struct cake_pile* inode_pile;
static struct cake_pile* file_pile;
static struct cake_pile* superblock_pile;

static struct v_superblock* root_sb;
static struct hbucket* dnode_cache;

static int fs_id = 0;

struct v_dnode*
vfs_d_alloc();

void
vfs_d_free(struct v_dnode* dnode);

struct v_superblock*
vfs_sb_alloc();

void
vfs_sb_free(struct v_superblock* sb);

void
vfs_init()
{
    // 为他们专门创建一个蛋糕堆，而不使用valloc，这样我们可以最小化内碎片的产生
    dnode_pile = cake_new_pile("dnode_cache", sizeof(struct v_dnode), 1, 0);
    inode_pile = cake_new_pile("inode_cache", sizeof(struct v_inode), 1, 0);
    inode_pile = cake_new_pile("file_cache", sizeof(struct v_file), 1, 0);
    superblock_pile =
      cake_new_pile("sb_cache", sizeof(struct v_superblock), 1, 0);

    dnode_cache = valloc(DNODE_HASHTABLE_SIZE * sizeof(struct hbucket));

    // 挂载Lunaix的根文件系统，TwiFS！
    struct filesystem* rootfs = fsm_get("twifs");
    root_sb = vfs_sb_alloc();
    rootfs->mount(root_sb, NULL);
}

inline struct hbucket*
__dcache_get_bucket(struct v_dnode* parent, unsigned int hash)
{
    // 与parent的指针值做加法，来减小碰撞的可能性。
    hash += (uint32_t)parent;
    // 确保低位更加随机
    hash = hash ^ (hash >> DNODE_HASHBITS);
    return &dnode_cache[hash & DNODE_HASH_MASK];
}

struct v_dnode*
vfs_dcache_lookup(struct v_dnode* parent, struct hstr* str)
{
    struct hbucket* slot = __dcache_get_bucket(parent, str->hash);

    struct v_dnode *pos, *n;
    hashtable_bucket_foreach(slot, pos, n, hash_list)
    {
        if (pos->name.hash == str->hash) {
            return pos;
        }
    }
    return NULL;
}

void
vfs_dcache_add(struct v_dnode* parent, struct v_dnode* dnode)
{
    struct hbucket* bucket = __dcache_get_bucket(parent, dnode->name.hash);
    hlist_add(&bucket->head, &dnode->hash_list);
}

int
vfs_walk(struct v_dnode* start,
         const char* path,
         struct v_dnode** dentry,
         int walk_options)
{
    int errno = 0;
    int i = 0, j = 0;

    if (path[0] == PATH_DELIM) {
        if ((walk_options & VFS_WALK_FSRELATIVE)) {
            start = start->super_block->root;
        } else {
            start = root_sb->root;
        }
        i++;
    }

    struct v_dnode* dnode;
    struct v_dnode* current_level = start;

    char name_content[VFS_NAME_MAXLEN];
    struct hstr name = HSTR(name_content, 0);

    char current = path[i++], lookahead;
    while (current) {
        lookahead = path[i++];
        if (current != PATH_DELIM) {
            if (j >= VFS_NAME_MAXLEN - 1) {
                return VFS_ETOOLONG;
            }
            if (!VFS_VALID_CHAR(current)) {
                return VFS_EINVLD;
            }
            name_content[j++] = current;
            if (lookahead) {
                goto cont;
            }
        }

        // handling cases like /^.*(\/+).*$/
        if (lookahead == PATH_DELIM) {
            goto cont;
        }

        name_content[j] = 0;
        hstr_rehash(&name, HSTR_FULL_HASH);

        dnode = vfs_dcache_lookup(current_level, &name);

        if (!dnode) {
            dnode = vfs_d_alloc();
            dnode->name = HSTR(valloc(VFS_NAME_MAXLEN), j);
            dnode->name.hash = name.hash;

            strcpy(dnode->name.value, name_content);

            errno =
              current_level->inode->ops.dir_lookup(current_level->inode, dnode);

            if (errno == VFS_ENOTFOUND &&
                ((walk_options & VFS_WALK_MKPARENT) ||
                 ((walk_options & VFS_WALK_MKDIR) && !lookahead))) {
                /*
                    两种创建文件夹的情况：
                        1) VFS_WALK_MKPARENT：沿路创建所有不存在的文件夹
                        2) VFS_WALK_MKDIR：仅创建路径最后一级的不存在文件夹
                */
                if (!current_level->inode->ops.mkdir) {
                    errno = VFS_ENOOPS;
                } else {
                    errno = current_level->inode->ops.mkdir(
                      current_level->inode, dnode);
                }
            }

            if (errno) {
                goto error;
            }

            vfs_dcache_add(current_level, dnode);

            dnode->parent = current_level;
            llist_append(&current_level->children, &dnode->siblings);
        }

        j = 0;
        current_level = dnode;
    cont:
        current = lookahead;
    };

    *dentry = current_level;
    return 0;

error:
    vfree(dnode->name.value);
    vfs_d_free(dnode);
    return errno;
}

int
vfs_mount(const char* fs_name, bdev_t device, struct v_dnode* mnt_point)
{
    struct filesystem* fs = fsm_get(fs_name);
    if (!fs)
        return VFS_ENOFS;
    struct v_superblock* sb = vfs_sb_alloc();
    sb->dev = device;
    sb->fs_id = fs_id++;

    int errno = 0;
    if (!(errno = fs->mount(sb, mnt_point))) {
        sb->fs = fs;
        sb->root = mnt_point;
        mnt_point->super_block = sb;
        llist_append(&root_sb->sb_list, &sb->sb_list);
    }

    return errno;
}

int
vfs_mkdir(const char* parent_path,
          const char* component,
          struct v_dnode** dentry)
{
    return vfs_walk(root_sb->root, parent_path, dentry, VFS_WALK_MKDIR);
}

int
vfs_unmount(struct v_dnode* mnt_point)
{
    int errno = 0;
    struct v_superblock* sb = mnt_point->super_block;
    if (!sb) {
        return VFS_EBADMNT;
    }
    if (!(errno = sb->fs->unmount(sb))) {
        struct v_dnode* fs_root = sb->root;
        llist_delete(&fs_root->siblings);
        llist_delete(&sb->sb_list);
        vfs_sb_free(sb);
    }
    return errno;
}

int
vfs_open(struct v_dnode* dnode, struct v_file** file)
{
    if (!dnode->inode || !dnode->inode->ops.open) {
        return VFS_ENOOPS;
    }

    struct v_file* vfile = cake_grab(file_pile);
    memset(vfile, 0, sizeof(*vfile));

    int errno = dnode->inode->ops.open(dnode->inode, vfile);
    if (errno) {
        cake_release(file_pile, vfile);
    } else {
        *file = vfile;
    }
    return errno;
}

int
vfs_close(struct v_file* file)
{
    if (!file->ops.close) {
        return VFS_ENOOPS;
    }

    int errno = file->ops.close(file);
    if (!errno) {
        cake_release(file_pile, file);
    }
    return errno;
}

int
vfs_fsync(struct v_file* file)
{
    int errno = VFS_ENOOPS;
    if (file->ops.sync) {
        errno = file->ops.sync(file);
    }
    if (!errno && file->inode->ops.sync) {
        return file->inode->ops.sync(file->inode);
    }
    return errno;
}

struct v_superblock*
vfs_sb_alloc()
{
    struct v_superblock* sb = cake_grab(superblock_pile);
    memset(sb, 0, sizeof(*sb));
    llist_init_head(&sb->sb_list);
    return sb;
}

void
vfs_sb_free(struct v_superblock* sb)
{
    cake_release(superblock_pile, sb);
}

struct v_dnode*
vfs_d_alloc()
{
    struct v_dnode* dnode = cake_grab(dnode_pile);
    llist_init_head(&dnode->children);
}

void
vfs_d_free(struct v_dnode* dnode)
{
    if (dnode->ops.destruct) {
        dnode->ops.destruct(dnode);
    }
    cake_release(dnode_pile, dnode);
}

struct v_inode*
vfs_i_alloc()
{
    struct v_inode* inode = cake_grab(inode_pile);
    memset(inode, 0, sizeof(*inode));

    return inode;
}

void
vfs_i_free(struct v_inode* inode)
{
    cake_release(inode_pile, inode);
}
