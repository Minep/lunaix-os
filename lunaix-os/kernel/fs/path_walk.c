#include <lunaix/fs.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/process.h>
#include <lunaix/spike.h>

#include <usr/lunaix/fcntl.h>

#include <klibc/string.h>

#define VFS_SYMLINK_DEPTH 16
#define VFS_SYMLINK_MAXLEN 512

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
        } 
        else if (unlikely(!__current)) {
            start = vfs_sysroot;
        }
        else {
            start = __current->root ?: vfs_sysroot;
        }

        if (unlikely(!start || !start->mnt)) {
            fail("vfs: no root");
        }

        if (path[0] == VFS_PATH_DELIM) {
            i++;
        }
    }

    assert(start);

    struct v_dnode* dnode;
    struct v_dnode* current_level = start;
    struct v_inode* current_inode = current_level->inode;

    struct hstr name = HSTR(fname_buffer, 0);

    char current = path[i++], lookahead;
    while (current) 
    {
        lookahead = path[i++];

        if (current != VFS_PATH_DELIM) 
        {
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

        lock_dnode(current_level);

        if (!check_allow_execute(current_inode)) {
            errno = EACCESS;
            unlock_dnode(current_level);
            goto error;
        }

        dnode = vfs_dcache_lookup(current_level, &name);

        if (!dnode) 
        {
            dnode = vfs_d_alloc(current_level, &name);

            if (!dnode) {
                errno = ENOMEM;
                unlock_dnode(current_level);
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
        current_inode = current_level->inode;

        assert(current_inode);
        
        if (check_symlink_node(current_inode) &&
            !(walk_options & VFS_WALK_NOFOLLOW)) 
        {
            const char* link;
            struct v_inode_ops* iops;

            iops = current_inode->ops;

            if (!iops->read_symlink) {
                errno = ENOTSUP;
                goto error;
            }

            lock_inode(current_inode);

            errno = iops->read_symlink(current_inode, &link);
            if ((errno < 0)) {
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

            current_level = dnode;
            current_inode = dnode->inode;
        }

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
    if (!path) {
        *dentry = NULL;
        return 0;
    }

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

int
vfs_walkat(int fd, const char* path, int at_opts, struct v_dnode** dnode_out)
{
    int errno, options = 0;
    struct v_dnode *root_dnode;
    struct v_fd* _fd;

    if ((at_opts & AT_FDCWD)) {
        root_dnode = __current->cwd;
    }
    else 
    {
        errno = vfs_getfd(fd, &_fd);
        if (errno) {
            return errno;
        }

        root_dnode = _fd->file->dnode;
    }

    if ((at_opts & AT_SYMLINK_NOFOLLOW)) {
        options |= VFS_WALK_NOFOLLOW;
    }

    errno = vfs_walk(root_dnode, path, dnode_out, NULL, options);
    if (errno) {
        return errno;
    }

    return 0;
}