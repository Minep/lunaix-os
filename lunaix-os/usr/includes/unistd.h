#ifndef __LUNAIX_SYS_UNISTD_H
#define __LUNAIX_SYS_UNISTD_H

#include <sys/types.h>

extern const char** environ;

pid_t
fork();

pid_t
getpid();

pid_t
getppid();

pid_t
getpgid();

pid_t
setpgid(pid_t pid, pid_t pgid);

int
brk(void* addr);

void*
sbrk(ssize_t size);

void
_exit(int status);

unsigned int
sleep(unsigned int);

int
pause();

int
kill(pid_t pid, int signum);

unsigned int
alarm(unsigned int seconds);

int
link(const char* oldpath, const char* newpath);

int
rmdir(const char* pathname);

int
read(int fd, void* buf, size_t size);

int
write(int fd, void* buf, size_t size);

int
readlink(const char* path, char* buffer, size_t size);

int
readlinkat(int dirfd, const char* pathname, char* buffer, size_t size);

int
lseek(int fd, off_t offset, int mode);

int
unlink(const char* pathname);

int
unlinat(int fd, const char* pathname);

int
mkdir(const char* path);

int
close(int fd);

int
dup2(int oldfd, int newfd);

int
dup(int oldfd);

int
fsync(int fd);

int
symlink(const char* pathname, const char* linktarget);

int
chdir(const char* path);

int
fchdir(int fd);

char*
getcwd(char* buf, size_t size);

int
rename(const char* oldpath, const char* newpath);

int
getxattr(const char* path, const char* name, void* value, size_t len);

int
setxattr(const char* path, const char* name, void* value, size_t len);

int
fgetxattr(int fd, const char* name, void* value, size_t len);

int
fsetxattr(int fd, const char* name, void* value, size_t len);

int
execve(const char* filename, const char* argv[], const char* envp[]);

#endif /* __LUNAIX_UNISTD_H */
