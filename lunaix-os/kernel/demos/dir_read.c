#include <lunaix/dirent.h>
#include <lunaix/fctrl.h>
#include <lunaix/proc.h>
#include <lunaix/syslog.h>

LOG_MODULE("RDDIR")

void
_readdir_main()
{
    int fd = open("/dev/./../dev/.", 0);
    if (fd == -1) {
        kprintf(KERROR "fail to open (%d)\n", geterrno());
        return;
    }

    struct dirent ent = { .d_offset = 0 };

    while (!readdir(fd, &ent)) {
        kprintf(KINFO "%s\n", ent.d_name);
    }

    char path[129];

    int len = readlinkat(fd, ".", path, 128);
    if (len < 0) {
        kprintf(KERROR "fail to read (%d)\n", geterrno());
    }

    path[len] = 0;

    kprintf("%s\n", path);

    close(fd);

    return;
}