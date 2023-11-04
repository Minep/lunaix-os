#include <errno.h>
#include <fcntl.h>
#include <lunaix/lunaix.h>
#include <lunaix/mount.h>
#include <stdio.h>
#include <unistd.h>

#define must_mount(src, target, fs, opts)                                      \
    do {                                                                       \
        int err = 0;                                                           \
        if ((err = mount(src, target, fs, opts))) {                            \
            syslog(2, "mount fs %s to %s failed (%d)\n", fs, target, errno);   \
            return err;                                                        \
        }                                                                      \
    } while (0)

int
main(int argc, const char** argv)
{
    int err = 0;

    mkdir("/dev");
    mkdir("/sys");
    mkdir("/task");

    must_mount(NULL, "/dev", "devfs", 0);
    must_mount(NULL, "/sys", "twifs", MNT_RO);
    must_mount(NULL, "/task", "taskfs", MNT_RO);

    if ((err = open("/dev/tty", 0)) < 0) {
        syslog(2, "fail to open tty (%d)\n", errno);
        return err;
    }

    if ((err = dup(err)) < 0) {
        syslog(2, "fail to setup tty i/o (%d)\n", errno);
        return err;
    }

    if ((err = symlink("/usr", "/mnt/lunaix-os/usr"))) {
        syslog(2, "symlink /usr:/mnt/lunaix-os/usr (%d)\n", errno);
        return err;
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

    return err;
}