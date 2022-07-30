#ifndef __LUNAIX_FCTRL_H
#define __LUNAIX_FCTRL_H

#include <lunaix/dirent.h>
#include <lunaix/syscall.h>
#include <stddef.h>

__LXSYSCALL2(int, open, const char*, path, int, options);

__LXSYSCALL1(int, close, int, fd);

__LXSYSCALL1(int, mkdir, const char*, path);
__LXSYSCALL1(int, rmdir, const char*, pathname);
__LXSYSCALL1(int, unlink, const char*, pathname);
__LXSYSCALL2(int, unlinkat, int, fd, const char*, pathname);

__LXSYSCALL2(int, readdir, int, fd, struct dirent*, dent);

__LXSYSCALL3(int, lseek, int, fd, int, offset, int, options);

__LXSYSCALL3(int, read, int, fd, void*, buf, unsigned int, count);

__LXSYSCALL3(int, write, int, fd, void*, buf, unsigned int, count);

__LXSYSCALL3(int, readlink, const char*, path, char*, buf, size_t, size);

__LXSYSCALL4(int,
             readlinkat,
             int,
             dirfd,
             const char*,
             pathname,
             char*,
             buf,
             size_t,
             size);

#endif /* __LUNAIX_FCTRL_H */
