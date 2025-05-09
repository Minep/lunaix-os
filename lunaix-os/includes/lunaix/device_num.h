#ifndef __LUNAIX_DEVICE_NUM_H
#define __LUNAIX_DEVICE_NUM_H

#include <lunaix/types.h>

/*
    Device metadata field (device_def::meta)

    31          16 15           0
    |  interface  |   function  |

    Where the interface identify how the device is connected with the processor
    Lunaix identify the following values:

        NON: device do not have hardware interfacing

        SOC: device conntected through some System-on-Chip interconnect bus
            for example, southbridge on x86 platform, AMBA for ARM's series.

        PCI: device connected through the peripheral component interconnect bus
            (PCI)

        USB: device connected through the universal serial bus (USB)

        SPI: device connected through the serial peripheral interface (SPI)

        I2C: device connected through the IIC protocol

        FMW: device is a system board firmware

    The function defines the functionality that the device is designated to
   serve. Lunaix identify the following values:

        PSEDUO: a pseudo device which does not backed by any hardware. (e.g.
                /dev/null)

        CHAR: a character device, which support read/write and dealing with
             characters. Backed hardware might exist.

        SERIAL: a serial interface which talks

        STORAGE: a device that is used for storage of data

        INPUT: a device that accept external input.

        TIME: a device that provides time related services, for example, timing
             and clocking

        BUSIF: a device that is the interface or HBAs for accessing interconnect
   bus.

        TTY: a device which can be called as teletypewriter, system can use such
            device for output into external environment

        CFG: device that provide configuration service to the system or other
   devices


*/

#define DEV_FNGRP(vendor, function)                                            \
    (((vendor) & 0xffff) << 16) | ((function) & 0xffff)
#define DEV_UNIQUE(devkind, variant)                                           \
    (((devkind) & 0xffff) << 16) | ((variant) & 0xffff)
#define DEV_KIND_FROM(unique) ((unique) >> 16)
#define DEV_VAR_FROM(unique) ((unique) & 0xffff)

#define DEV_VN(fngrp) ((fngrp) >> 16)
#define DEV_FN(fngrp) (((fngrp) & 0xffff))

#define dev_vn(x)      DEVVN_##x
#define dev_fn(x)      DEVFN_##x
#define dev_id(x)        DEV_##x

enum devnum_vn
{
    dev_vn(GENERIC),
    #include <listings/devnum_vn.lst>
};

enum devnum_fn
{
    dev_fn(NON),
    #include <listings/devnum_fn.lst>
};

enum devnum
{
    dev_id(NON),
    #include <listings/devnum.lst>
};

struct devident
{
    u32_t fn_grp;
    u32_t unique;
};

struct devclass
{
    u32_t fn_grp;
    u32_t device;
    u32_t variant;
};

static inline int
devclass_mkvar(struct devclass* class)
{
    return class->variant++;
}

#endif /* __LUNAIX_DEVICE_NUM_H */
