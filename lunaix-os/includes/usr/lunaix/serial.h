#ifndef __LUNAIX_USERIAL_H
#define __LUNAIX_USERIAL_H

#include "ioctl_defs.h"

#define SERIO_RXEN IOREQ(1, 0)
#define SERIO_RXDA IOREQ(2, 0)

#define SERIO_TXEN IOREQ(3, 0)
#define SERIO_TXDA IOREQ(4, 0)

#endif /* __LUNAIX_USERIAL_H */
