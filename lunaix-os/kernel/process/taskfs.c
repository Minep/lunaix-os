#include <lunaix/fs/taskfs.h>
#include <lunaix/fs/twimap.h>
#include <lunaix/fs/api.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>

#include <klibc/strfmt.h>
#include <klibc/string.h>

#include <usr/lunaix/dirent_defs.h>

#define COUNTER_MASK ((1 << 16) - 1)

static struct hbucket* attr_export_table;
static DEFINE_LIST(attributes);
static volatile int ino_cnt = 1;

extern struct scheduler sched_ctx;

static void
__destruct_inode(struct v_inode* inode)
{
    vfree_safe(inode->data);
}

static int
taskfs_next_counter()
{
    return (ino_cnt = ((ino_cnt + 1) & COUNTER_MASK) + 1);
}

static inode_t
taskfs_inode_id(pid_t pid, int sub_counter)
{
    return ((pid & (MAX_PROCESS - 1)) << 16) | (sub_counter & COUNTER_MASK);
}

int
taskfs_mknod(struct v_dnode* dnode, pid_t pid, int sub_counter, int itype)
{
    inode_t ino = taskfs_inode_id(pid, sub_counter);

    struct v_superblock* vsb = dnode->super_block;
    struct v_inode* inode = vfs_i_find(vsb, ino);
    if (!inode) {
        if (!(inode = vfs_i_alloc(vsb))) {
            return ENOMEM;
        }

        inode->id = ino;
        inode->itype = itype;
        inode->destruct = __destruct_inode;
        vfs_i_addhash(inode);
    }

    vfs_assign_inode(dnode, inode);
    return 0;
}

static inline int
__report_task_entries(struct v_file* file, struct dir_context* dctx)
{
    unsigned int counter = 0;
    char name[VFS_NAME_MAXLEN];
    struct proc_info *pos, *n;

    llist_for_each(pos, n, sched_ctx.proc_list, tasks)
    {
        if (!fsapi_readdir_pos_entries_at(file, counter)) {
            counter++;
            continue;
        }

        ksnprintf(name, VFS_NAME_MAXLEN, "%d", pos->pid);
        dctx->read_complete_callback(dctx, name, VFS_NAME_MAXLEN, DT_DIR);
        return 1;
    }

    return 0;
}

static inline int
__report_task_attributes(struct v_file* file, struct dir_context* dctx)
{
    unsigned int counter = 0;
    struct task_attribute *pos, *n;

    list_for_each(pos, n, attributes.first, siblings)
    {
        if (fsapi_readdir_pos_entries_at(file, counter)) {
            dctx->read_complete_callback(
                dctx, HSTR_VAL(pos->key), VFS_NAME_MAXLEN, DT_FILE);
            return 1;
        }
        counter++;
    }

    return 0;
}

int
taskfs_readdir(struct v_file* file, struct dir_context* dctx)
{
    struct v_inode* inode = file->inode;
    pid_t pid = inode->id >> 16;
    unsigned int counter = 0;

    if ((inode->id & COUNTER_MASK)) {
        return ENOTDIR;
    }

    if (fsapi_handle_pseudo_dirent(file, dctx)) {
        return 1;
    }

    if (pid) {
        return __report_task_attributes(file, dctx);
    }

    return __report_task_entries(file, dctx);
}

// ascii to pid
static inline pid_t
taskfs_atop(const char* str)
{
    pid_t t = 0;
    int i = 0;
    char c;
    while ((c = str[i++])) {
        if ('0' > c || c > '9') {
            return -1;
        }
        t = t * 10 + (c - '0');
    }
    return t;
}

static inline int
__init_task_inode(pid_t pid, struct v_dnode* dnode)
{
    struct twimap* map;
    struct proc_info* proc;
    struct task_attribute* tattr;
    
    tattr = taskfs_get_attr(&dnode->name);
    if (!tattr || !(proc = get_process(pid)))
        return ENOENT;

    map = twimap_create(proc);
    map->ops = tattr->ops;

    dnode->inode->data = map;
    dnode->inode->default_fops = &twimap_file_ops;
    
    return 0;
}

static int
__lookup_attribute(pid_t pid, struct v_dnode* dnode)
{
    int errno;    

    errno = taskfs_mknod(dnode, pid, taskfs_next_counter(), VFS_IFFILE);
    if (errno) {
        return errno;
    }

    return __init_task_inode(pid, dnode);
}

int
taskfs_dirlookup(struct v_inode* this, struct v_dnode* dnode)
{
    pid_t pid = this->id >> 16;

    if (pid) {
        return __lookup_attribute(pid, dnode);
    }

    pid = taskfs_atop(dnode->name.value);

    if (pid <= 0) {
        return ENOENT;
    }

    return taskfs_mknod(dnode, pid, 0, VFS_IFDIR);
}

static struct v_file_ops taskfs_file_ops = { 
    .close =        default_file_close,
    .read =         default_file_read,
    .read_page =    default_file_read_page,
    .write =        default_file_write,
    .write_page =   default_file_write_page,
    .readdir =      taskfs_readdir,
    .seek =         default_file_seek 
};

static struct v_inode_ops taskfs_inode_ops = { 
    .dir_lookup =   taskfs_dirlookup,
    .open =         default_inode_open,
    .mkdir =        default_inode_mkdir,
    .rmdir =        default_inode_rmdir,
    .rename =       default_inode_rename 
};

static volatile struct v_superblock* taskfs_sb;

void
taskfs_init_inode(struct v_superblock* vsb, struct v_inode* inode)
{
    inode->default_fops = &taskfs_file_ops;
    inode->ops = &taskfs_inode_ops;
}

int
taskfs_mount(struct v_superblock* vsb, struct v_dnode* mount_point)
{
    taskfs_sb = vsb;
    vsb->ops.init_inode = taskfs_init_inode;

    return taskfs_mknod(mount_point, 0, 0, VFS_IFDIR);
}

int
taskfs_unmount(struct v_superblock* vsb)
{
    return 0;
}

void
taskfs_invalidate(pid_t pid)
{
    struct v_dnode *pos, *n;
    struct v_inode* inode;

    inode = vfs_i_find(taskfs_sb, taskfs_inode_id(pid, 0));
    if (!inode)
        return;

    llist_for_each(pos, n, &inode->aka_dnodes, aka_list)
    {
        if (pos->ref_count > 1) {
            continue;
        }
        vfs_d_free(pos);
    }

    if (!inode->link_count) {
        vfs_i_free(inode);
    }
}

#define ATTR_TABLE_LEN 16
#define ATTR_KEY_LEN 32

void
taskfs_export_attr_mapping(const char* key, struct twimap_ops ops)
{
    char* key_val;
    struct hbucket* slot;
    struct task_attribute* tattr;
    
    tattr = valloc(sizeof(*tattr));
    key_val = valloc(ATTR_KEY_LEN);

    tattr->ops = ops;
    tattr->key = HSTR(key_val, 0);
    strncpy(key_val, key, ATTR_KEY_LEN);
    hstr_rehash(&tattr->key, HSTR_FULL_HASH);

    slot = &attr_export_table[tattr->key.hash % ATTR_TABLE_LEN];
    hlist_add(&slot->head, &tattr->attrs);
    list_add(&attributes, &tattr->siblings);
}

struct task_attribute*
taskfs_get_attr(struct hstr* key)
{
    struct hbucket* slot = &attr_export_table[key->hash % ATTR_TABLE_LEN];
    struct task_attribute *pos, *n;
    hashtable_bucket_foreach(slot, pos, n, attrs)
    {
        if (HSTR_EQ(&pos->key, key)) {
            return pos;
        }
    }
    return NULL;
}

extern void
export_task_attr();

void
taskfs_init()
{
    struct filesystem* fs;
    fs = fsapi_fs_declare("taskfs", FSTYPE_PSEUDO);
    
    fsapi_fs_set_mntops(fs, taskfs_mount, taskfs_unmount);
    fsapi_fs_finalise(fs);

    attr_export_table = vcalloc(ATTR_TABLE_LEN, sizeof(struct hbucket));

    export_task_attr();
}
EXPORT_FILE_SYSTEM(taskfs, taskfs_init);