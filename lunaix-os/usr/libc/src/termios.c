#include <termios.h>
#include <lunaix/ioctl.h>

int
tcgetattr(int fd, struct termios* termios_p) 
{
    return ioctl(fd, TDEV_TCGETATTR, termios_p);
}


int
tcsendbreak(int fd, int ) 
{
    // TODO
    return 0;
}

int
tcsetattr(int fd, int optional_actions, const struct termios* termios_p)
{
    return ioctl(fd, TDEV_TCSETATTR, termios_p);
}
