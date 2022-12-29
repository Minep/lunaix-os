#include <lunaix/input.h>

#include <ulibc/stdio.h>
#include <usr/errno.h>
#include <usr/fcntl.h>
#include <usr/sys/lunaix.h>
#include <usr/unistd.h>

#define STDIN 1
#define STDOUT 0

void
input_test()
{
    int fd = open("/dev/input/i8042-kbd", 0);

    if (fd < 0) {
        printf("fail to open (%d)", errno);
        return;
    }

    struct input_evt_pkt event;

    while (read(fd, &event, sizeof(event)) > 0) {
        char* action;
        if (event.pkt_type == PKT_PRESS) {
            action = "pressed";
        } else {
            action = "release";
        }

        printf("%u: %s '%c', class=0x%x, scan=%x\n",
               event.timestamp,
               action,
               event.sys_code & 0xff,
               (event.sys_code & 0xff00) >> 8,
               event.scan_code);
    }
    return;
}