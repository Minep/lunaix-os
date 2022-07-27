#include <lunaix/dirent.h>
#include <lunaix/fctrl.h>
#include <lunaix/syslog.h>

LOG_MODULE("RDDIR")

void
_readdir_main()
{
    int fd = open("/bus/../..", 0);
    if (fd == -1) {
        kprintf(KERROR "fail to open\n");
        return;
    }

    struct dirent ent = { .d_offset = 0 };

    while (!readdir(fd, &ent)) {
        kprintf(KINFO "%s\n", ent.d_name);
    }

    close(fd);

    return;
}