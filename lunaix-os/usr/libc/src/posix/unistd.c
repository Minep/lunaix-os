#include <syscall.h>
#include <lunaix/types.h>

pid_t
fork()
{
    return do_lunaix_syscall(__NR__lxsys_fork);
}

int
brk(void* addr)
{
    return do_lunaix_syscall(__NR__lxsys_brk, addr);
}

void*
sbrk(ssize_t size)
{
    return (void*)do_lunaix_syscall(__NR__lxsys_sbrk, size);
}

pid_t
getpid()
{
    return do_lunaix_syscall(__NR__lxsys_getpid);
}

pid_t
getppid()
{
    return do_lunaix_syscall(__NR__lxsys_getppid);
}

pid_t
getpgid()
{
    return do_lunaix_syscall(__NR__lxsys_getpgid);
}

pid_t
setpgid(pid_t pid, pid_t pgid)
{
    return do_lunaix_syscall(__NR__lxsys_setpgid, pid, pgid);
}
void
_exit(int status)
{
    do_lunaix_syscall(__NR__lxsys_exit, status);
}

unsigned int
sleep(unsigned int seconds)
{
    return do_lunaix_syscall(__NR__lxsys_sleep, seconds);
}

int
pause()
{
    return do_lunaix_syscall(__NR__lxsys_pause);
}

unsigned int
alarm(unsigned int seconds)
{
    return do_lunaix_syscall(__NR__lxsys_alarm, seconds);
}

int
link(const char* oldpath, const char* newpath)
{
    return do_lunaix_syscall(__NR__lxsys_link, oldpath, newpath);
}

int
rmdir(const char* pathname)
{
    return do_lunaix_syscall(__NR__lxsys_rmdir, pathname);
}

int
read(int fd, void* buf, size_t count)
{
    return do_lunaix_syscall(__NR__lxsys_read, fd, buf, count);
}

int
write(int fd, void* buf, size_t count)
{
    return do_lunaix_syscall(__NR__lxsys_write, fd, buf, count);
}

int
readlink(const char* path, char* buf, size_t size)
{
    return do_lunaix_syscall(__NR__lxsys_readlink, path, buf, size);
}

int
lseek(int fd, off_t offset, int options)
{
    return do_lunaix_syscall(__NR__lxsys_lseek, fd, offset, options);
}

int
unlink(const char* pathname)
{
    return do_lunaix_syscall(__NR__lxsys_unlink, pathname);
}

int
close(int fd)
{
    return do_lunaix_syscall(__NR__lxsys_close, fd);
}

int
dup2(int oldfd, int newfd)
{
    return do_lunaix_syscall(__NR__lxsys_dup2, oldfd, newfd);
}

int
dup(int oldfd)
{
    return do_lunaix_syscall(__NR__lxsys_dup, oldfd);
}

int
fsync(int fildes)
{
    return do_lunaix_syscall(__NR__lxsys_fsync, fildes);
}

int
symlink(const char* pathname, const char* link_target)
{
    return do_lunaix_syscall(__NR__lxsys_symlink, pathname, link_target);
}

int
chdir(const char* path)
{
    return do_lunaix_syscall(__NR__lxsys_chdir, path);
}

int
fchdir(int fd)
{
    return do_lunaix_syscall(__NR__lxsys_fchdir, fd);
}

char*
getcwd(char* buf, size_t size)
{
    return (char*)do_lunaix_syscall(__NR__lxsys_getcwd, buf, size);
}

int
rename(const char* oldpath, const char* newpath)
{
    return do_lunaix_syscall(__NR__lxsys_rename, oldpath, newpath);
}

int
getxattr(const char* path, const char* name, void* value, size_t len)
{
    return do_lunaix_syscall(__NR__lxsys_getxattr, path, name, value, len);
}

int
setxattr(const char* path, const char* name, void* value, size_t len)
{
    return do_lunaix_syscall(__NR__lxsys_setxattr, path, name, value, len);
}

int
fgetxattr(int fd, const char* name, void* value, size_t len)
{
    return do_lunaix_syscall(__NR__lxsys_fgetxattr, fd, name, value, len);
}

int
fsetxattr(int fd, const char* name, void* value, size_t len)
{
    return do_lunaix_syscall(__NR__lxsys_fsetxattr, fd, name, value, len);
}

int
readlinkat(int dirfd, const char* pathname, char* buf, size_t size)
{
    return do_lunaix_syscall(__NR__lxsys_readlinkat, dirfd, pathname, buf, size);
}

int
unlinkat(int fd, const char* pathname)
{
    return do_lunaix_syscall(__NR__lxsys_unlinkat, fd, pathname);
}

int
mkdir(const char* path)
{
    return do_lunaix_syscall(__NR__lxsys_mkdir, path);
}

int
execve(const char* filename, const char** argv, const char** envp)
{
    return do_lunaix_syscall(__NR__lxsys_execve, filename, argv, envp);
}
