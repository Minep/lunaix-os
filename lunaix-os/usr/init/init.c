#include <errno.h>
#include <fcntl.h>
#include <lunaix/lunaix.h>
#include <lunaix/mount.h>
#include <termios.h>
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

#define check(statement)                                                       \
    ({                                                                         \
        int err = 0;                                                           \
        if ((err = (statement)) < 0) {                                         \
            syslog(2, #statement " failed: %d", err);                          \
            _exit(1);                                                          \
        }                                                                      \
        err;                                                                   \
    })

int
init_termios(int fd) {
    struct termios term;

    check(tcgetattr(fd, &term));

    term.c_lflag = ICANON | IEXTEN | ISIG | ECHO | ECHOE | ECHONL;
    term.c_iflag = ICRNL | IGNBRK;
    term.c_oflag = ONLCR | OPOST;
    term.c_cflag = CREAD | CLOCAL | CS8 | CPARENB;
    term.c_cc[VERASE] = 0x7f;
    
    cfsetispeed(&term, B9600);
    cfsetospeed(&term, B9600);

    check(tcsetattr(fd, 0, &term));

    return 0;
}

const char* sh_argv[] = { "/usr/bin/sh", 0  };
const char* sh_envp[] = {  0  };

int
main(int argc, const char** argv)
{
    mkdir("/dev");
    mkdir("/sys");
    mkdir("/task");

    must_mount(NULL, "/dev", "devfs", 0);
    must_mount(NULL, "/sys", "twifs", MNT_RO);
    must_mount(NULL, "/task", "taskfs", MNT_RO);

    int fd = check(open("/dev/tty", 0));

    check(init_termios(fd));

    check(dup(fd));

    check(symlink("/usr", "/mnt/lunaix-os/usr"));

    pid_t pid;
    int err = 0;
    if (!(pid = fork())) {

        
        err = execve(sh_argv[0], sh_argv, sh_envp);
        printf("fail to execute (%d)\n", errno);
        _exit(err);
    }

    waitpid(pid, &err, 0);

    if (WEXITSTATUS(err)) {
        printf("shell exit abnormally (%d)\n", err);
    }

    printf("init exiting\n");

    return err;
}