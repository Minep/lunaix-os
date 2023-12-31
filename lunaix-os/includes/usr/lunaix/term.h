#ifndef __LUNAIX_UTERM_H
#define __LUNAIX_UTERM_H

#define _BRKINT (1)
#define _ICRNL (1 << 1)
#define _IGNBRK (1 << 2)
#define _IGNCR (1 << 3)
#define _IGNPAR (1 << 4)
#define _INLCR (1 << 5)
#define _INPCK (1 << 6)
#define _ISTRIP (1 << 7)
#define _IUCLC (1 << 8)
#define _IXANY (1 << 9)
#define _IXOFF (1 << 10)
#define _IXON (1 << 11)
#define _PARMRK (1 << 12)

#define _OPOST 1
#define _OCRNL _ICRNL

// 2
// 3
// 4

#define _ONLRET _INLCR
#define _ONLCR (1 << 6)

// 7

#define _OLCUC _IUCLC // 8

#define _ECHO (1)
#define _ECHOE (1 << 1)
#define _ECHOK (1 << 2)
#define _ECHONL (1 << 3)
#define _ICANON (1 << 4)
#define _ISIG (1 << 5)
#define _IEXTEN (1 << 6)
#define _NOFLSH (1 << 7)

#define _VEOF 0
#define _VEOL 1
#define _VERASE 2
#define _VINTR 3
#define _VKILL 4
#define _VQUIT 5
#define _VSUSP 6
#define _VSTART 7
#define _VSTOP 8
#define _VMIN 9
#define _VTIME 10
#define _NCCS 11

#define _B50 50
#define _B75 75
#define _B110 110
#define _B134 134
#define _B150 150
#define _B200 200
#define _B300 300
#define _B600 600
#define _B1200 1200
#define _B1800 1800
#define _B2400 2400
#define _B4800 4800
#define _B9600 9600
#define _B19200 19200
#define _B38400 38400

#define _CLOCAL 1
#define _CREAD (1 << 1)
#define _CSZ_MASK (0b11 << 2)
#define _CS5 (0b00 << 2)
#define _CS6 (0b01 << 2)
#define _CS7 (0b10 << 2)
#define _CS8 (0b11 << 2)
#define _CSTOPB (1 << 4)
#define _CHUPCL (1 << 5)
#define _CPARENB (1 << 6)
#define _CPARODD (1 << 7)

#define _TCSANOW 1
#define _TCSADRAIN 2
#define _TCSAFLUSH 3

#define TDEV_TCGETATTR 1
#define TDEV_TCSETATTR 2
#define TDEV_TCPUSHLC 3
#define TDEV_TCPOPLC 4
#define TDEV_TCSETCHDEV 5
#define TDEV_TCGETCHDEV 6

typedef int tcflag_t;
typedef char cc_t;
typedef unsigned int speed_t;

struct termios
{
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_cc[_NCCS];
    speed_t c_baud;
};

#endif /* __LUNAIX_UTERM_H */
