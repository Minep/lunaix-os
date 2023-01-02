#include <fcntl.h>
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

    syslog(0, "(%p) user space!\n", main);

    return 0;
}