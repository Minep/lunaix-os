#include <lunaix/keyboard.h>
#include <lunaix/lxconsole.h>
#include <lunaix/proc.h>

void
_pconsole_main()
{
    struct kdb_keyinfo_pkt keyevent;
    while (1) {
        if (!kbd_recv_key(&keyevent)) {
            yield();
            continue;
        }
        if ((keyevent.state & KBD_KEY_FPRESSED)) {
            if (keyevent.keycode == KEY_UP) {
                console_view_up();
            } else if (keyevent.keycode == KEY_DOWN) {
                console_view_down();
            }
        }
    }
}