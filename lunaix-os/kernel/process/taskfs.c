#include <lunaix/dirent.h>
#include <lunaix/fs/taskfs.h>
#include <lunaix/fs/twimap.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/process.h>
#include <lunaix/sched.h>

#include <klibc/stdio.h>
#include <klibc/string.h>

#define COUNTER_MASK ((1 << 16) - 1)

static struct hbucket* attr_export_table;
static DEFINE_LLIST(attributes);
static volatile int ino_cnt = 1;

int
taskfs_next_counter()
{
    return (ino_cnt = ((ino_cnt + 1) & COUNTER_MASK) + 1);
}

inode_t
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
        vfs_i_addhash(inode);
    }

    vfs_assign_inode(dnode, inode);
    return 0;
}

int
taskfs_readdir(struct v_file* file, struct dir_context* dctx)
{
    struct v_inode* inode = file->inode;
    pid_t pid = inode->id >> 16;
    int counter = 0;

    if ((inode->id & COUNTER_MASK)) {
        return ENOTDIR;
    }

    if (pid) {
        struct task_attribute *pos, *n;
        llist_for_each(pos, n, &attributes, siblings)
        {
            if (counter == dctx->index) {
                dctx->read_complete_callback(
                  dctx, pos->key_val, VFS_NAME_MAXLEN, DT_FILE);
                return 1;
            }
            counter++;
        }
        return 0;
    }

    char name[VFS_NAME_MAXLEN];
    struct proc_info *root = get_process(pid), *pos, *n;
    llist_for_each(pos, n, &root->tasks, tasks)
    {
        if (counter == dctx->index) {
            ksnprintf(name, VFS_NAME_MAXLEN, "%d", pos->pid);
            dctx->read_complete_callback(dctx, name, VFS_NAME_MAXLEN, DT_DIR);
            return 1;
        }
        counter++;
    }
    return 0;
}

// ascii to pid
pid_t
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

int
taskfs_dirlookup(struct v_inode* this, struct v_dnode* dnode)
{
    pid_t pid = this->id >> 16;
    struct proc_info* proc;

    if (pid) {
        struct task_attribute* tattr = taskfs_get_attr(&dnode->name);
        if (!tattr || !(proc = get_process(pid)))
            return ENOENT;

        int errno =
          taskfs_mknod(dnode, pid, taskfs_next_counter(), VFS_IFSEQDEV);
        if (!errno) {
            tattr->map_file->data = proc;
            dnode->inode->data = tattr->map_file;
            dnode->inode->default_fops = &twimap_file_ops;
        }
        return errno;
    }

    pid = taskfs_atop(dnode->name.value);

    if (pid <= 0 || !(proc = get_process(pid))) {
        return ENOENT;
    }

    return taskfs_mknod(dnode, pid, 0, VFS_IFDIR);
}

static struct v_file_ops taskfs_file_ops = { .close = default_file_close,
                                             .read = default_file_read,
                                             .read_page = default_file_read,
                                             .write = default_file_write,
                                             .write_page = default_file_write,
                                             .readdir = taskfs_readdir,
                                             .seek = default_file_seek };
static struct v_inode_ops taskfs_inode_ops = { .dir_lookup = taskfs_dirlookup,
                                               .open = default_inode_open,
                                               .mkdir = default_inode_mkdir,
                                               .rmdir = default_inode_rmdir,
                                               .rename = default_inode_rename };

void
taskfs_init_inode(struct v_superblock* vsb, struct v_inode* inode)
{
    inode->default_fops = &taskfs_file_ops;
    inode->ops = &taskfs_inode_ops;
}

static volatile struct v_superblock* taskfs_sb;

int
taskfs_mount(struct v_superblock* vsb, struct v_dnode* mount_point)
{
    taskfs_sb = vsb;
    vsb->ops.init_inode = taskfs_init_inode;

    return taskfs_mknod(mount_point, 0, 0, VFS_IFDIR);
}

void
taskfs_invalidate(pid_t pid)
{
    struct v_dnode *pos, *n;
    struct v_inode* inode = vfs_i_find(taskfs_sb, taskfs_inode_id(pid, 0));

    if (!inode)
        return;

    llist_for_each(pos, n, &inode->aka_dnodes, aka_list)
    {
        if (pos->ref_count > 1) {
            continue;
        }
        vfs_d_free(pos);
    }
}

#define ATTR_TABLE_LEN 16

void
taskfs_export_attr(const char* key, struct twimap* attr_map_file)
{
    struct task_attribute* tattr = valloc(sizeof(*tattr));

    tattr->map_file = attr_map_file;
    tattr->key = HSTR(tattr->key_val, 0);
    strncpy(tattr->key_val, key, 32);
    hstr_rehash(&tattr->key, HSTR_FULL_HASH);

    struct hbucket* slot = &attr_export_table[tattr->key.hash % ATTR_TABLE_LEN];
    hlist_add(&slot->head, &tattr->attrs);
    llist_append(&attributes, &tattr->siblings);
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
    struct filesystem* taskfs = fsm_new_fs("taskfs", 5);
    taskfs->mount = taskfs_mount;

    fsm_register(taskfs);

    attr_export_table = vcalloc(ATTR_TABLE_LEN, sizeof(struct hbucket));

    export_task_attr();
}