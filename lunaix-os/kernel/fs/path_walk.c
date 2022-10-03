#include <lunaix/fs.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

#include <klibc/string.h>

#define VFS_SYMLINK_DEPTH 16

extern struct lru_zone *dnode_lru, *inode_lru;

int
__vfs_walk(struct v_dnode* start,
           const char* path,
           struct v_dnode** dentry,
           struct hstr* component,
           int walk_options,
           size_t depth,
           char* fname_buffer)
{
    int errno = 0;
    int i = 0, j = 0;

    if (depth >= VFS_SYMLINK_DEPTH) {
        return ELOOP;
    }

    if (path[0] == VFS_PATH_DELIM || !start) {
        if ((walk_options & VFS_WALK_FSRELATIVE) && start) {
            start = start->super_block->root;
        } else {
            start = vfs_sysroot;
            if (!vfs_sysroot->mnt) {
                panick("vfs: no root");
            }
        }

        if (path[0] == VFS_PATH_DELIM) {
            i++;
        }
    }

    assert(start);

    struct v_dnode* dnode;
    struct v_inode* current_inode;
    struct v_dnode* current_level = start;

    struct hstr name = HSTR(fname_buffer, 0);

    char current = path[i++], lookahead;
    while (current) {
        lookahead = path[i++];
        if (current != VFS_PATH_DELIM) {
            if (j >= VFS_NAME_MAXLEN - 1) {
                return ENAMETOOLONG;
            }
            if (!VFS_VALID_CHAR(current)) {
                return EINVAL;
            }
            fname_buffer[j++] = current;
            if (lookahead) {
                goto cont;
            }
        }

        // handling cases like /^.*(\/+).*$/
        if (lookahead == VFS_PATH_DELIM) {
            goto cont;
        }

        fname_buffer[j] = 0;
        name.len = j;
        hstr_rehash(&name, HSTR_FULL_HASH);

        if (!lookahead && (walk_options & VFS_WALK_PARENT)) {
            if (component) {
                hstrcpy(component, &name);
            }
            break;
        }

        current_inode = current_level->inode;

        if ((current_inode->itype & VFS_IFSYMLINK) &&
            !(walk_options & VFS_WALK_NOFOLLOW)) {
            const char* link;

            lock_inode(current_inode);
            if ((errno =
                   current_inode->ops->read_symlink(current_inode, &link))) {
                unlock_inode(current_inode);
                goto error;
            }
            unlock_inode(current_inode);

            errno = __vfs_walk(current_level->parent,
                               link,
                               &dnode,
                               NULL,
                               0,
                               depth + 1,
                               fname_buffer + name.len + 1);

            if (errno) {
                goto error;
            }

            // reposition the resolved subtree pointed by symlink
            // vfs_dcache_rehash(current_level->parent, dnode);
            current_level = dnode;
            current_inode = dnode->inode;
        }

        lock_dnode(current_level);

        dnode = vfs_dcache_lookup(current_level, &name);

        if (!dnode) {
            dnode = vfs_d_alloc(current_level, &name);

            if (!dnode) {
                errno = ENOMEM;
                goto error;
            }

            lock_inode(current_inode);

            errno = current_inode->ops->dir_lookup(current_inode, dnode);

            if (errno == ENOENT && (walk_options & VFS_WALK_MKPARENT)) {
                if (!current_inode->ops->mkdir) {
                    errno = ENOTSUP;
                } else {
                    errno = current_inode->ops->mkdir(current_inode, dnode);
                }
            }

            vfs_dcache_add(current_level, dnode);
            unlock_inode(current_inode);

            if (errno) {
                unlock_dnode(current_level);
                goto cleanup;
            }
        }

        unlock_dnode(current_level);

        j = 0;
        current_level = dnode;
    cont:
        current = lookahead;
    };

    *dentry = current_level;
    return 0;

cleanup:
    vfs_d_free(dnode);
error:
    *dentry = NULL;
    return errno;
}

int
vfs_walk(struct v_dnode* start,
         const char* path,
         struct v_dnode** dentry,
         struct hstr* component,
         int options)
{
    // allocate a file name stack for path walking and recursion to resolve
    // symlink
    char* name_buffer = valloc(2048);

    int errno =
      __vfs_walk(start, path, dentry, component, options, 0, name_buffer);

    vfree(name_buffer);
    return errno;
}

int
vfs_walk_proc(const char* path,
              struct v_dnode** dentry,
              struct hstr* component,
              int options)
{
    return vfs_walk(__current->cwd, path, dentry, component, options);
}