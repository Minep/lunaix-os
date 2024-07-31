#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define BUFSIZE 4096

static char buffer[BUFSIZE];

#define _open(f, o)                                                 \
    ({                                                              \
        int __fd = open(f, o);                                      \
        if (__fd < 0) {                                             \
            printf("open failed: %s (error: %d)", f, errno);   \
            _exit(__fd);                                            \
        }                                                           \
        __fd;                                                       \
    })

int
main(int argc, const char* argv[])
{
    int fd = 0, fdrand = 0;
    int size = 0, sz2 = 0;

    fd = _open(argv[1], O_RDWR | O_CREAT);
    fdrand = _open("/dev/rand", O_RDONLY);

    size = read(fdrand, buffer, BUFSIZE);

    for (int i = 0; i < size; i++)
    {
        buffer[i] = (char)((buffer[i] % 94) + 33);
    }

    buffer[size - 1] = 0;
    
    sz2 = write(fd, buffer, size - 1);
    sz2 += write(fd, "\n\n", 2);
    sz2 += write(fd, buffer, size - 1);

    fsync(fd);
    close(fd);
    close(fdrand);

    return 0;
}