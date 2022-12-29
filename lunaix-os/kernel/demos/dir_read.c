#include <usr/errno.h>
#include <usr/fcntl.h>
#include <usr/sys/dirent.h>
#include <usr/unistd.h>

void
_readdir_main()
{
    int fd = open("/dev/./../dev/.", 0);
    if (fd == -1) {
        printf("fail to open (%d)\n", errno);
        return;
    }

    char path[129];
    int len = realpathat(fd, path, 128);
    if (len < 0) {
        printf("fail to read (%d)\n", errno);
    } else {
        path[len] = 0;
        printf("%s\n", path);
    }

    struct lx_dirent ent = { .d_offset = 0 };

    while (sys_readdir(fd, &ent) == 1) {
        printf("%s\n", ent.d_name);
    }

    close(fd);

    return;
}