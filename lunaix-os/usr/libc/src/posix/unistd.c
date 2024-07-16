#include <lunaix/syscall.h>
#include <lunaix/types.h>

pid_t
fork()
{
    return do_lunaix_syscall(__SYSCALL_fork);
}

int
brk(void* addr)
{
    return do_lunaix_syscall(__SYSCALL_brk, addr);
}

void*
sbrk(ssize_t size)
{
    return (void*)do_lunaix_syscall(__SYSCALL_sbrk, size);
}

pid_t
getpid()
{
    return do_lunaix_syscall(__SYSCALL_getpid);
}

pid_t
getppid()
{
    return do_lunaix_syscall(__SYSCALL_getppid);
}

pid_t
getpgid()
{
    return do_lunaix_syscall(__SYSCALL_getpgid);
}

pid_t
setpgid(pid_t pid, pid_t pgid)
{
    return do_lunaix_syscall(__SYSCALL_setpgid, pid, pgid);
}
void
_exit(int status)
{
    do_lunaix_syscall(__SYSCALL__exit, status);
}

unsigned int
sleep(unsigned int seconds)
{
    return do_lunaix_syscall(__SYSCALL_sleep, seconds);
}

int
pause()
{
    return do_lunaix_syscall(__SYSCALL_pause);
}

unsigned int
alarm(unsigned int seconds)
{
    return do_lunaix_syscall(__SYSCALL_alarm, seconds);
}

int
link(const char* oldpath, const char* newpath)
{
    return do_lunaix_syscall(__SYSCALL_link, oldpath, newpath);
}

int
rmdir(const char* pathname)
{
    return do_lunaix_syscall(__SYSCALL_rmdir, pathname);
}

int
read(int fd, void* buf, size_t count)
{
    return do_lunaix_syscall(__SYSCALL_read, fd, buf, count);
}

int
write(int fd, void* buf, size_t count)
{
    return do_lunaix_syscall(__SYSCALL_write, fd, buf, count);
}

int
readlink(const char* path, char* buf, size_t size)
{
    return do_lunaix_syscall(__SYSCALL_readlink, path, buf, size);
}

int
lseek(int fd, off_t offset, int options)
{
    return do_lunaix_syscall(__SYSCALL_lseek, fd, offset, options);
}

int
unlink(const char* pathname)
{
    return do_lunaix_syscall(__SYSCALL_unlink, pathname);
}

int
close(int fd)
{
    return do_lunaix_syscall(__SYSCALL_close, fd);
}

int
dup2(int oldfd, int newfd)
{
    return do_lunaix_syscall(__SYSCALL_dup2, oldfd, newfd);
}

int
dup(int oldfd)
{
    return do_lunaix_syscall(__SYSCALL_dup, oldfd);
}

int
fsync(int fildes)
{
    return do_lunaix_syscall(__SYSCALL_fsync, fildes);
}

int
symlink(const char* pathname, const char* link_target)
{
    return do_lunaix_syscall(__SYSCALL_symlink, pathname, link_target);
}

int
chdir(const char* path)
{
    return do_lunaix_syscall(__SYSCALL_chdir, path);
}

int
fchdir(int fd)
{
    return do_lunaix_syscall(__SYSCALL_fchdir, fd);
}

char*
getcwd(char* buf, size_t size)
{
    return (char*)do_lunaix_syscall(__SYSCALL_getcwd, buf, size);
}

int
rename(const char* oldpath, const char* newpath)
{
    return do_lunaix_syscall(__SYSCALL_rename, oldpath, newpath);
}

int
getxattr(const char* path, const char* name, void* value, size_t len)
{
    return do_lunaix_syscall(__SYSCALL_getxattr, path, name, value, len);
}

int
setxattr(const char* path, const char* name, void* value, size_t len)
{
    return do_lunaix_syscall(__SYSCALL_setxattr, path, name, value, len);
}

int
fgetxattr(int fd, const char* name, void* value, size_t len)
{
    return do_lunaix_syscall(__SYSCALL_fgetxattr, fd, name, value, len);
}

int
fsetxattr(int fd, const char* name, void* value, size_t len)
{
    return do_lunaix_syscall(__SYSCALL_fsetxattr, fd, name, value, len);
}

int
readlinkat(int dirfd, const char* pathname, char* buf, size_t size)
{
    return do_lunaix_syscall(__SYSCALL_readlinkat, dirfd, pathname, buf, size);
}

int
unlinkat(int fd, const char* pathname)
{
    return do_lunaix_syscall(__SYSCALL_unlinkat, fd, pathname);
}

int
mkdir(const char* path)
{
    return do_lunaix_syscall(__SYSCALL_mkdir, path);
}

int
execve(const char* filename, const char** argv, const char** envp)
{
    return do_lunaix_syscall(__SYSCALL_execve, filename, argv, envp);
}
