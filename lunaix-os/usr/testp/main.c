#include <errno.h>
#include <fcntl.h>
#include <lunaix/lunaix.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void
test_serial()
{
    int fd = open("/dev/ttyS0", FO_WRONLY);
    if (fd < 0) {
        printf("ttyS0 not accessable (%d)\n", errno);
        return;
    }

    char buf[32];
    int sz = 0;

    printf("ttyS0 input: ");

    if ((sz = read(fd, buf, 31)) < 0) {
        printf("write to ttyS0 failed (%d)\n", errno);
    }

    buf[sz] = 0;

    printf("%s", buf);

    close(fd);
}

int
main(int argc, char* argv[])
{
    if (argc <= 1) {
        return 1;
    }

    char* target = argv[1];

    if (!strcmp(target, "serial")) {
        test_serial();
    } else {
        printf("unknown test: %s\n", target);
        return 1;
    }

    return 0;
}