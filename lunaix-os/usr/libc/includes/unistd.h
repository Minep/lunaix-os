#ifndef __LUNAIX_SYS_UNISTD_H
#define __LUNAIX_SYS_UNISTD_H

#include <lunaix/types.h>
#include <stddef.h>

extern const char** environ;

extern pid_t
fork();

extern pid_t
getpid();

extern pid_t
getppid();

extern pid_t
getpgid();

extern pid_t
setpgid(pid_t pid, pid_t pgid);

extern int
brk(void* addr);

extern void*
sbrk(ssize_t size);

extern void
_exit(int status);

extern unsigned int
sleep(unsigned int);

extern int
pause();

extern unsigned int
alarm(unsigned int seconds);

extern int
link(const char* oldpath, const char* newpath);

extern int
rmdir(const char* pathname);

extern int
read(int fd, void* buf, size_t size);

extern int
write(int fd, void* buf, size_t size);

extern int
readlink(const char* path, char* buffer, size_t size);

extern int
readlinkat(int dirfd, const char* pathname, char* buffer, size_t size);

extern int
lseek(int fd, off_t offset, int mode);

extern int
unlink(const char* pathname);

extern int
unlinat(int fd, const char* pathname);

extern int
mkdir(const char* path);

extern int
close(int fd);

extern int
dup2(int oldfd, int newfd);

extern int
dup(int oldfd);

extern int
fsync(int fd);

extern int
symlink(const char* pathname, const char* linktarget);

extern int
chdir(const char* path);

extern int
fchdir(int fd);

extern char*
getcwd(char* buf, size_t size);

extern int
rename(const char* oldpath, const char* newpath);

extern int
getxattr(const char* path, const char* name, void* value, size_t len);

extern int
setxattr(const char* path, const char* name, void* value, size_t len);

extern int
fgetxattr(int fd, const char* name, void* value, size_t len);

extern int
fsetxattr(int fd, const char* name, void* value, size_t len);

extern int
execve(const char* filename, const char* argv[], const char* envp[]);

#endif /* __LUNAIX_UNISTD_H */
