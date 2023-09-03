#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define BUFSIZE 4096

static char buffer[BUFSIZE];

int
main(int argc, const char* argv[])
{
    int fd = 0;
    unsigned int size = 0;
    for (int i = 1; i < argc; i++) {
        fd = open(argv[i], FO_RDONLY);
        if (fd < 0) {
            printf("open failed: %s (error: %d)", argv[i], fd);
            continue;
        }

        do {
            size = read(fd, buffer, BUFSIZE);
            write(stdout, buffer, size);
        } while (size == BUFSIZE);

        close(fd);
    }

    return 0;
}