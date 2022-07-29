#include <lunaix/fctrl.h>
#include <lunaix/foptions.h>
#include <lunaix/proc.h>
#include <lunaix/syslog.h>

LOG_MODULE("IOTEST")

void
_iotest_main()
{
    char test_sequence[] = "Once upon a time, in a magical land of Equestria. "
                           "There were two regal sisters who ruled together "
                           "and created harmony for all the land.";
    int fd = open("/dev/block/bd0", 0);

    if (fd < 0) {
        kprintf(KERROR "fail to open (%d)\n", geterrno());
        return;
    }

    lseek(fd, 1, FSEEK_SET);
    write(fd, test_sequence, sizeof(test_sequence));

    lseek(fd, -1, FSEEK_CUR);
    char read_out[256];
    read(fd, read_out, sizeof(read_out));

    kprintf("%s", read_out);
}