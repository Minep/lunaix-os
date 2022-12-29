#include <lunaix/syscall.h>
#include <usr/unistd.h>

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

__LXSYSCALL3(int, read, int, fd, void*, buf, size_t, count)

__LXSYSCALL3(int, write, int, fd, void*, buf, size_t, count)

__LXSYSCALL3(int, readlink, const char*, path, char*, buf, size_t, size)

__LXSYSCALL3(int, lseek, int, fd, off_t, offset, int, options)

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

__LXSYSCALL2(int, unlinkat, int, fd, const char*, pathname)

__LXSYSCALL1(int, mkdir, const char*, path)