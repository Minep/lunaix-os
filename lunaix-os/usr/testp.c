#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void
test_serial(char* dev, int wr)
{
    int fd = open(dev, FO_WRONLY);
    if (fd < 0) {
        printf("tty %s not accessable (%d)\n", dev, errno);
        return;
    }

    int sz = 0;
    char buf[256];

    if (!wr) {
        printf("tty input: ");

        if ((sz = read(fd, buf, 31)) < 0) {
            printf("read to tty failed (%d)\n", errno);
        }

        buf[sz] = 0;

        printf("%s", buf);
    }
    else {
        int size = snprintf(buf, 256, "serial test: %s", dev);
        write(fd, buf, size);
    }

    close(fd);
}

int
main(int argc, char* argv[])
{
    if (argc != 2) {
        printf("usage: testp path_to_dev\n");
        return 1;
    }

    char* target = argv[1];

    test_serial(target, 1);

    return 0;
}