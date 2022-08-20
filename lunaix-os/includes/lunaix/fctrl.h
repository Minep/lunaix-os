#ifndef __LUNAIX_FCTRL_H
#define __LUNAIX_FCTRL_H

#include <lunaix/dirent.h>
#include <lunaix/syscall.h>
#include <stddef.h>

__LXSYSCALL2(int, open, const char*, path, int, options)

__LXSYSCALL1(int, mkdir, const char*, path)
__LXSYSCALL2(int, unlinkat, int, fd, const char*, pathname)

__LXSYSCALL2(int, readdir, int, fd, struct dirent*, dent)

__LXSYSCALL4(int,
             readlinkat,
             int,
             dirfd,
             const char*,
             pathname,
             char*,
             buf,
             size_t,
             size)

__LXSYSCALL3(int, realpathat, int, fd, char*, buf, size_t, size)

__LXSYSCALL4(int,
             mount,
             const char*,
             source,
             const char*,
             target,
             const char*,
             fstype,
             int,
             options)

__LXSYSCALL1(int, unmount, const char*, target)

__LXSYSCALL4(int,
             getxattr,
             const char*,
             path,
             const char*,
             name,
             void*,
             value,
             size_t,
             len)

__LXSYSCALL4(int,
             setxattr,
             const char*,
             path,
             const char*,
             name,
             void*,
             value,
             size_t,
             len)

__LXSYSCALL4(int,
             fgetxattr,
             int,
             fd,
             const char*,
             name,
             void*,
             value,
             size_t,
             len)

__LXSYSCALL4(int,
             fsetxattr,
             int,
             fd,
             const char*,
             name,
             void*,
             value,
             size_t,
             len)

#endif /* __LUNAIX_FCTRL_H */
