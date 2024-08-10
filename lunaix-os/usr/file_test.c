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
    
    for (int i = 0; i < 100; i++)
    {
        printf("write to file: (round) {%d}/100\n", i + 1);
        
        size = read(fdrand, buffer, BUFSIZE);
        printf(">>> read random chars: %d\n", size);
        for (int i = 0; i < size; i++)
        {
            buffer[i] = (char)((unsigned char)(buffer[i] % 94U) + 33U);
        }
        
        sz2 += write(fd, buffer, size);
        sz2 += write(fd, "\n\n", 2);
    }

    close(fd);
    close(fdrand);

    return 0;
}