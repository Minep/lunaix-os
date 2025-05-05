#ifndef __LUNALIBC_TERMIOS_H
#define __LUNALIBC_TERMIOS_H

#include <lunaix/term.h>

#define BRKINT _BRKINT
#define ICRNL _ICRNL
#define IGNBRK _IGNBRK
#define IGNCR _IGNCR
#define IGNPAR _IGNPAR
#define INLCR _INLCR
#define INPCK _INPCK
#define ISTRIP _ISTRIP
#define IUCLC _IUCLC
#define IXANY _IXANY
#define IXOFF _IXOFF
#define IXON _IXON
#define PARMRK _PARMRK

#define OPOST _OPOST
#define OCRNL _OCRNL

// 2
// 3
// 4

#define ONLRET _ONLRET
#define ONLCR _ONLCR

// 7

#define OLCUC _OLCUC

#define ECHO _ECHO
#define ECHOE _ECHOE
#define ECHOK _ECHOK
#define ECHONL _ECHONL
#define ICANON _ICANON
#define ISIG _ISIG
#define IEXTEN _IEXTEN
#define NOFLSH _NOFLSH

#define VEOF _VEOF
#define VEOL _VEOL
#define VERASE _VERASE
#define VINTR _VINTR
#define VKILL _VKILL
#define VQUIT _VQUIT
#define VSUSP _VSUSP
#define VSTART _VSTART
#define VSTOP _VSTOP
#define VMIN _VMIN
#define VTIME _VTIME
#define NCCS _NCCS

#define B50 _B50
#define B75 _B75
#define B110 _B110
#define B134 _B134
#define B150 _B150
#define B200 _B200
#define B300 _B300
#define B600 _B600
#define B1200 _B1200
#define B1800 _B1800
#define B2400 _B2400
#define B4800 _B4800
#define B9600 _B9600
#define B19200 _B19200
#define B38400 _B38400

#define CLOCAL _CLOCAL
#define CREAD _CREAD
#define CS5 _CS5
#define CS6 _CS6
#define CS7 _CS7
#define CS8 _CS8
#define CSTOPB _CSTOPB
#define CHUPCL _CHUPCL
#define CPARENB _CPARENB
#define CPARODD _CPARODD

#define TCSANOW _TCSANOW
#define TCSADRAIN _TCSADRAIN
#define TCSAFLUSH _TCSAFLUSH

static inline speed_t cfgetispeed(const struct termios* termios) { return termios->c_baud; }
static inline speed_t cfgetospeed(const struct termios* termios) { return termios->c_baud; }

static inline int cfsetispeed(struct termios* termios, speed_t baud)
{
    if (baud > B38400) {
        return -1;
    }

    termios->c_baud = baud;
    return 0;
}

static inline int cfsetospeed(struct termios* termios, speed_t baud)
{
    if (baud > B38400) {
        return -1;
    }

    termios->c_baud = baud;
    return 0;
}


// TODO
//int     tcdrain(int);
//int     tcflow(int, int);
//int     tcflush(int, int);
// pid_t   tcgetsid(int);

int     tcgetattr(int, struct termios *);
int     tcsendbreak(int, int);
int     tcsetattr(int, int, const struct termios *);

#endif /* __LUNALIBC_TERMIOS_H */
