#include <fcntl.h>
#include <stdio.h>
#include <sys/lunaix.h>
#include <unistd.h>

int
main(int argc, const char** argv)
{
    int errno = 0;

    if ((errno = open("/dev/tty", 0)) < 0) {
        syslog(2, "fail to open tty (%d)\n", errno);
        return 0;
    }

    if ((errno = dup(errno)) < 0) {
        syslog(2, "fail to setup tty i/o (%d)\n", errno);
        return 0;
    }

    printf("(%p) user space!\n", (void*)main);

    pid_t pid;
    if (!(pid = fork())) {
        int err = execve("/mnt/lunaix-os/usr/ls", NULL, NULL);
        printf("fail to execute (%d)\n", err);
        _exit(err);
    }

    waitpid(pid, NULL, 0);

    return 0;
}