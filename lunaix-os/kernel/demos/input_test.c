#include <lunaix/fctrl.h>
#include <lunaix/foptions.h>
#include <lunaix/input.h>
#include <lunaix/lunistd.h>

#define STDIN 1
#define STDOUT 0

void
input_test()
{
    int fd = open("/dev/input/i8042-kbd", 0);

    if (fd < 0) {
        write(STDOUT, "fail to open", 13);
        return;
    }

    struct input_evt_pkt event;

    while (read(fd, &event, sizeof(event)) > 0) {
        if (event.pkt_type == PKT_PRESS) {
            write(STDOUT, "PRESSED: ", 10);
        } else {
            write(STDOUT, "RELEASE: ", 10);
        }
        char c = event.sys_code & 0xff;
        write(STDOUT, &c, 1);
        write(STDOUT, "\n", 2);
    }
    return;
}