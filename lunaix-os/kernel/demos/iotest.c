#include <lunaix/fctrl.h>
#include <lunaix/foptions.h>
#include <lunaix/lunistd.h>
#include <lunaix/proc.h>
#include <lunaix/syslog.h>

LOG_MODULE("IOTEST")

void
_iotest_main()
{
    char test_sequence[] = "Once upon a time, in a magical land of Equestria. "
                           "There were two regal sisters who ruled together "
                           "and created harmony for all the land.";

    int fd = open("/dev/sda", 0); // sda 设备 - 硬盘

    // tty 设备 - 控制台，使用O_DIRECT打开，即所有IO绕过Lunaix内核的缓存机制
    int tty = open("/dev/tty", FO_DIRECT);

    if (fd < 0 || tty < 0) {
        kprintf(KERROR "fail to open (%d)\n", geterrno());
        return;
    }

    // 移动指针至512字节，在大多数情况下，这是第二个逻辑扇区的起始处
    lseek(fd, 512, FSEEK_SET);
    write(fd, test_sequence, sizeof(test_sequence));
    lseek(fd, 521, FSEEK_SET);
    write(fd, test_sequence, sizeof(test_sequence));

    // 读出我们写的内容
    char read_out[256];
    lseek(fd, 512, FSEEK_SET);
    read(fd, read_out, sizeof(read_out));

    // 将读出的内容直接写入tty设备
    write(tty, read_out, sizeof(read_out));
    write(tty, "\n", 1);

    close(fd);
    close(tty);

    kprint_hex(read_out, sizeof(read_out));
}