#include <errno.h>
#include <fcntl.h>
#include <lunaix/lunaix.h>
#include <stdio.h>
#include <unistd.h>

int
main(int argc, const char** argv)
{
    int err = 0;

    if ((err = open("/dev/tty", 0)) < 0) {
        syslog(2, "fail to open tty (%d)\n", errno);
        return 0;
    }

    if ((err = dup(err)) < 0) {
        syslog(2, "fail to setup tty i/o (%d)\n", errno);
        return 0;
    }

    if ((err = symlink("/usr", "/mnt/lunaix-os/usr"))) {
        syslog(2, "symlink /usr:/mnt/lunaix-os/usr (%d)\n", errno);
        return 0;
    }

    pid_t pid;
    if (!(pid = fork())) {
        err = execve("/usr/bin/sh", NULL, NULL);
        printf("fail to execute (%d)\n", errno);
        _exit(err);
    }

    waitpid(pid, &err, 0);

    if (WEXITSTATUS(err)) {
        printf("shell exit abnormally (%d)", err);
    }

    return 0;
}