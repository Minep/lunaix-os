#ifndef _LUNAIX_UHDR_USERIAL_H
#define _LUNAIX_UHDR_USERIAL_H

#include "ioctl.h"

#define SERIO_RXEN          IOREQ(1, 0)
#define SERIO_RXDA          IOREQ(2, 0)

#define SERIO_TXEN          IOREQ(3, 0)
#define SERIO_TXDA          IOREQ(4, 0)

#define SERIO_SETBRDRATE    IOREQ(5, 0)
#define SERIO_SETCNTRLMODE  IOREQ(6, 0)
#define SERIO_SETBRDBASE    IOREQ(7, 0)

#endif /* _LUNAIX_UHDR_USERIAL_H */
