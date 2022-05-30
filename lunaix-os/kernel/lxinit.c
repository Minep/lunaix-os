#include <hal/cpu.h>
#include <lunaix/syslog.h>
#include <lunaix/mm/kalloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/spike.h>
#include <lunaix/clock.h>
#include <lunaix/timer.h>
#include <lunaix/keyboard.h>
#include <lunaix/tty/tty.h>
#include <lunaix/lunistd.h>
#include <lunaix/proc.h>

extern uint8_t __kernel_start;

LOG_MODULE("INIT")

void 
test_timer(void* payload);

void
_lxinit_main()
{
    // 这里是就是LunaixOS的第一个进程了！
    for (size_t i = 0; i < 10; i++)
    {
        pid_t pid = 0;
        if (!(pid = fork())) {
            while (1)
            {
                // kprintf(KINFO "Process %d\n", i);
                tty_put_char('0'+i);
                yield();
            }
        }
        kprintf(KINFO "Forked %d\n", pid);
    }

    char buf[64];

    kprintf(KINFO "Hello higher half kernel world!\nWe are now running in virtual "
           "address space!\n\n");

    cpu_get_brand(buf);
    kprintf("CPU: %s\n\n", buf);

    void* k_start = vmm_v2p(&__kernel_start);
    kprintf(KINFO "The kernel's base address mapping: %p->%p\n", &__kernel_start, k_start);

    // no lxmalloc here! This can only be used within kernel, but here, we are in a dedicated process!
    // any access to kernel method must be done via syscall

    struct kdb_keyinfo_pkt keyevent;
    while (1)
    {
        if (!kbd_recv_key(&keyevent)) {
            // yield();
            continue;
        }
        if ((keyevent.state & KBD_KEY_FPRESSED) && (keyevent.keycode & 0xff00) <= KEYPAD) {
            tty_put_char((char)(keyevent.keycode & 0x00ff));
            tty_sync_cursor();
        }
    }
    

    spin();
}

static datetime_t datetime;

void test_timer(void* payload) {
    clock_walltime(&datetime);

    kprintf(KWARN "%u/%02u/%02u %02u:%02u:%02u\r",
           datetime.year,
           datetime.month,
           datetime.day,
           datetime.hour,
           datetime.minute,
           datetime.second);
}