#include <lunaix/dirent.h>
#include <lunaix/fctrl.h>
#include <lunaix/lunaix.h>
#include <lunaix/lunistd.h>

void
_readdir_main()
{
    int fd = open("/dev/./../dev/.", 0);
    if (fd == -1) {
        printf("fail to open (%d)\n", geterrno());
        return;
    }

    char path[129];
    int len = realpathat(fd, path, 128);
    if (len < 0) {
        printf("fail to read (%d)\n", geterrno());
    } else {
        path[len] = 0;
        printf("%s\n", path);
    }

    struct dirent ent = { .d_offset = 0 };

    while (readdir(fd, &ent) == 1) {
        printf("%s\n", ent.d_name);
    }

    close(fd);

    return;
}