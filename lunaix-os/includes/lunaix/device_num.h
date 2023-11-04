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

#define DEV_FNGRP(if_, function)                                               \
    (((if_) & 0xffff) << 16) | ((function) & 0xffff)
#define DEV_UNIQUE(devkind, variant)                                           \
    (((devkind) & 0xffff) << 16) | ((variant) & 0xffff)
#define DEV_KIND_FROM(unique) ((unique) >> 16)
#define DEV_VAR_FROM(unique) ((unique) & 0xffff)

#define DEV_IF(fngrp) ((fngrp) >> 16)
#define DEV_FN(fngrp) (((fngrp) & 0xffff))

#define DEVIF_NON 0x0
#define DEVIF_SOC 0x1
#define DEVIF_PCI 0x2
#define DEVIF_USB 0x3
#define DEVIF_SPI 0x4
#define DEVIF_I2C 0x5
#define DEVIF_FMW 0x6

#define DEVFN_PSEUDO 0x0
#define DEVFN_CHAR 0x1
#define DEVFN_STORAGE 0x4
#define DEVFN_INPUT 0x5
#define DEVFN_TIME 0x6
#define DEVFN_BUSIF 0x7
#define DEVFN_TTY 0x8
#define DEVFN_DISP 0x9
#define DEVFN_CFG 0xa

#define DEV_BUILTIN 0
#define DEV_BUILTIN_NULL 0
#define DEV_BUILTIN_ZERO 1

#define DEV_VTERM 1
#define DEV_RNG 2
#define DEV_RTC 3
#define DEV_SATA 4
#define DEV_NVME 5
#define DEV_PCI 6
#define DEV_UART16550 7

#define DEV_TIMER 8
#define DEV_TIMER_APIC 0
#define DEV_TIMER_HEPT 1

#define DEV_NULL 9
#define DEV_ZERO 10
#define DEV_KBD 11
#define DEV_GFXA 12
#define DEV_VGA 13
#define DEV_ACPI 14

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
    u32_t hash;
};

#endif /* __LUNAIX_DEVICE_NUM_H */
