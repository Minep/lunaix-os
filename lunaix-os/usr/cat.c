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
    int size = 0;
    struct file_stat stat;
    for (int i = 1; i < argc; i++) {
        fd = open(argv[i], FO_RDONLY);
                
        if (fd < 0) {
            printf("open failed: %s (error: %d)", argv[i], fd);
            continue;
        }

        if (fstat(fd, &stat) < 0) {
            printf("fail to get stat %d\n", errno);
            return 1;
        }

        if ((stat.mode & F_DIR)) {
            printf("%s is a directory", argv[i]);
            return 1;
        }

        do {
            size = read(fd, buffer, BUFSIZE);
            write(stdout, buffer, size);
        } while (size == BUFSIZE);

        close(fd);
    }

    return 0;
}