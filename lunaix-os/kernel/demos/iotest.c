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

    // sda 设备 - 硬盘
    //  sda设备属于容积设备（Volumetric Device），
    //  Lunaix会尽可能缓存任何对此设备的上层读写，并使用延迟写入策略。（FO_DIRECT可用于屏蔽该功能）
    int fd = open("/dev/sda", 0);

    // tty 设备 - 控制台。
    //  tty设备属于序列设备（Sequential Device），该类型设备的上层读写
    //  无须经过Lunaix的缓存层，而是直接下发到底层驱动。（不受FO_DIRECT的影响）
    int tty = open("/dev/tty", 0);

    if (fd < 0 || tty < 0) {
        kprintf(KERROR "fail to open (%d)\n", geterrno());
        return;
    }

    // 移动指针至512字节，在大多数情况下，这是第二个逻辑扇区的起始处
    lseek(fd, 512, FSEEK_SET);

    // 总共写入 64 * 136 字节，会产生3个页作为缓存
    for (size_t i = 0; i < 64; i++) {
        write(fd, test_sequence, sizeof(test_sequence));
    }

    lseek(fd, 521, FSEEK_SET);
    write(fd, test_sequence, sizeof(test_sequence));

    char read_out[256];
    write(tty, "input: ", 8);
    int size = read(tty, read_out, 256);

    write(tty, "your input: ", 13);
    write(tty, read_out, size);
    write(fd, read_out, size);
    write(tty, "\n", 1);

    // 读出我们写的内容
    lseek(fd, 512, FSEEK_SET);
    read(fd, read_out, sizeof(read_out));

    // 将读出的内容直接写入tty设备
    write(tty, read_out, sizeof(read_out));
    write(tty, "\n", 1);

    // 关闭文件，这同时会将页缓存中的数据下发到底层驱动
    close(fd);
    close(tty);

    kprint_hex(read_out, sizeof(read_out));
}