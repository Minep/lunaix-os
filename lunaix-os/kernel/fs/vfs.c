#include <klibc/string.h>
#include <lunaix/fs.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/valloc.h>

#define PATH_DELIM '/'
#define DNODE_HASHTABLE_BITS 10
#define DNODE_HASHTABLE_SIZE (1 << DNODE_HASHTABLE_BITS)
#define DNODE_HASH_MASK (DNODE_HASHTABLE_SIZE - 1)
#define DNODE_HASHBITS (32 - DNODE_HASHTABLE_BITS)

#define VFS_ETOOLONG -1
#define VFS_ENOFS -2

static struct cake_pile* dnode_pile;
static struct cake_pile* inode_pile;
static struct cake_pile* superblock_pile;
static struct cake_pile* name_pile;

static struct v_superblock* root_sb;
static struct hbucket* dnode_cache;

static int fs_id = 0;

struct v_dnode*
__new_dnode();

void
__free_dnode(struct v_dnode* dnode);

struct v_superblock*
__new_superblock();

void
__free_superblock(struct v_superblock* sb);

void
vfs_init()
{
    // 为他们专门创建一个蛋糕堆，而不使用valloc，这样我们可以最小化内部碎片的产生
    dnode_pile = cake_new_pile("dnode", sizeof(struct v_dnode), 1, 0);
    inode_pile = cake_new_pile("inode", sizeof(struct v_inode), 1, 0);
    name_pile = cake_new_pile("dname", VFS_NAME_MAXLEN, 1, 0);
    superblock_pile =
      cake_new_pile("superblock", sizeof(struct v_superblock), 1, 0);

    dnode_cache = valloc(DNODE_HASHTABLE_SIZE * sizeof(struct hbucket));

    // 挂载Lunaix的根文件系统，TwiFS！
    struct filesystem* rootfs = fsm_get("twifs");
    root_sb = __new_superblock();
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
vfs_walk(struct v_dnode* start, const char* path, struct v_dnode** dentry)
{
    int errno = 0;
    int i = 0, j = 0;

    if (path[0] == PATH_DELIM) {
        start = start->super_block->root;
        i++;
    }

    struct v_dnode* dnode;
    struct v_dnode* current_level = start;

    char name_content[VFS_NAME_MAXLEN];
    struct hstr name = HSTR(name_content, 0);

    char current, lookahead;
    do {
        current = path[i++];
        lookahead = path[i++];
        if (current != PATH_DELIM) {
            if (j >= VFS_NAME_MAXLEN - 1) {
                errno = VFS_ETOOLONG;
                goto error;
            }
            name_content[j++] = current;
            if (lookahead) {
                continue;
            }
        }

        name_content[j] = 0;
        hstr_rehash(&name, 32);

        dnode = vfs_dcache_lookup(current_level, &name);

        if (!dnode) {
            dnode = __new_dnode();
            strcpy(dnode->name.value, name_content);
            dnode->name.hash = name.hash;
            dnode->name.len = j;

            if ((errno =
                   dnode->inode->ops.dir_lookup(current_level->inode, dnode))) {
                goto error;
            }

            vfs_dcache_add(current_level, dnode);

            dnode->parent = current_level;
            llist_append(&current_level->children, &dnode->siblings);
        }

        j = 0;
        current_level = dnode;
    } while (lookahead);

    *dentry = dnode;
    return 0;

error:
    __free_dnode(dnode);
    return errno;
}

int
vfs_mount(const char* fs_name, bdev_t device, struct v_dnode* mnt_point)
{
    struct filesystem* fs = fsm_get(fs_name);
    if (!fs)
        return VFS_ENOFS;
    struct v_superblock* sb = __new_superblock();
    sb->dev = device;
    sb->fs_id = fs_id++;

    int errno = 0;
    if (!(errno = fs->mount(sb, mnt_point))) {
        sb->fs = fs;
        llist_append(&root_sb->sb_list, &sb->sb_list);
    }

    return errno;
}

int
vfs_unmount(const int fs_id)
{
    int errno = 0;
    struct v_superblock *pos, *n;
    llist_for_each(pos, n, &root_sb->sb_list, sb_list)
    {
        if (pos->fs_id != fs_id) {
            continue;
        }
        if (!(errno = pos->fs->unmount(pos))) {
            llist_delete(&pos->sb_list);
            __free_superblock(pos);
        }
        break;
    }
    return errno;
}

struct v_superblock*
__new_superblock()
{
    struct v_superblock* sb = cake_grab(superblock_pile);
    memset(sb, 0, sizeof(*sb));
    llist_init_head(&sb->sb_list);
    sb->root = __new_dnode();
}

void
__free_superblock(struct v_superblock* sb)
{
    __free_dnode(sb->root);
    cake_release(superblock_pile, sb);
}

struct v_dnode*
__new_dnode()
{
    struct v_dnode* dnode = cake_grab(dnode_pile);
    dnode->inode = cake_grab(inode_pile);
    dnode->name.value = cake_grab(name_pile);
}

void
__free_dnode(struct v_dnode* dnode)
{
    cake_release(inode_pile, dnode->inode);
    cake_release(name_pile, dnode->name.value);
    cake_release(dnode_pile, dnode);
}