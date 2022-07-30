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

    int fd = open("/dev/sda", 0);  // bd0 设备 - 硬盘
    int tty = open("/dev/tty", 0); // tty 设备 - 控制台

    if (fd < 0 || tty < 0) {
        kprintf(KERROR "fail to open (%d)\n", geterrno());
        return;
    }

    // 移动指针至第二个逻辑扇区（LBA=1），并写入
    lseek(fd, 1, FSEEK_SET);
    write(fd, test_sequence, sizeof(test_sequence));

    // 读出我们写的内容
    char read_out[256];
    lseek(fd, -1, FSEEK_CUR);
    read(fd, read_out, sizeof(read_out));

    write(tty, read_out, sizeof(read_out));

    close(fd);
    close(tty);

    kprint_hex(read_out, sizeof(read_out));
}