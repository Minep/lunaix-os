#ifndef __LUNAIX_UNISTD_H
#define __LUNAIX_UNISTD_H

#include <lunaix/syscall.h>
#include <lunaix/types.h>
#include <stddef.h>

__LXSYSCALL(pid_t, fork)

__LXSYSCALL1(int, sbrk, void*, addr)

__LXSYSCALL1(void*, brk, unsigned long, size)

__LXSYSCALL(pid_t, getpid)

__LXSYSCALL(pid_t, getppid)

__LXSYSCALL(pid_t, getpgid)

__LXSYSCALL2(pid_t, setpgid, pid_t, pid, pid_t, pgid)

__LXSYSCALL1(void, _exit, int, status)

__LXSYSCALL1(unsigned int, sleep, unsigned int, seconds)

__LXSYSCALL(int, pause)

__LXSYSCALL2(int, kill, pid_t, pid, int, signum)

__LXSYSCALL1(unsigned int, alarm, unsigned int, seconds)

__LXSYSCALL2(int, link, const char*, oldpath, const char*, newpath)

__LXSYSCALL1(int, rmdir, const char*, pathname)

__LXSYSCALL3(int, read, int, fd, void*, buf, unsigned int, count)

__LXSYSCALL3(int, write, int, fd, void*, buf, unsigned int, count)

__LXSYSCALL3(int, readlink, const char*, path, char*, buf, size_t, size)

__LXSYSCALL3(int, lseek, int, fd, int, offset, int, options)

__LXSYSCALL1(int, unlink, const char*, pathname)

__LXSYSCALL1(int, close, int, fd)

__LXSYSCALL2(int, dup2, int, oldfd, int, newfd)

__LXSYSCALL1(int, dup, int, oldfd)

__LXSYSCALL1(int, fsync, int, fildes)

__LXSYSCALL2(int, symlink, const char*, pathname, const char*, link_target)

__LXSYSCALL1(int, chdir, const char*, path)

__LXSYSCALL1(int, fchdir, int, fd)

__LXSYSCALL2(char*, getcwd, char*, buf, size_t, size)

__LXSYSCALL2(int, rename, const char*, oldpath, const char*, newpath)

#endif /* __LUNAIX_UNISTD_H */
