#ifndef __LUNAIX_DEVICE_NUM_H
#define __LUNAIX_DEVICE_NUM_H

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
*/

#define DEV_META(if_, function) (((if_) & 0xffff) << 16) | ((function) & 0xffff)
#define DEV_IF(meta) ((meta) >> 16)
#define DEV_FN(meta) (((meta) & 0xffff))

#define DEVIF_NON 0x0
#define DEVIF_SOC 0x1
#define DEVIF_PCI 0x2
#define DEVIF_USB 0x3
#define DEVIF_SPI 0x4
#define DEVIF_I2C 0x5

#define DEVFN_PSEUDO 0x0
#define DEVFN_CHAR 0x1
#define DEVFN_STORAGE 0x4
#define DEVFN_INPUT 0x5
#define DEVFN_TIME 0x6
#define DEVFN_BUSIF 0x7
#define DEVFN_TTY 0x8

#define DEV_BUILTIN 0x0
#define DEV_X86LEGACY 0x1
#define DEV_RNG 0x2
#define DEV_RTC 0x3
#define DEV_SATA 0x4
#define DEV_NVME 0x5
#define DEV_BUS 0x6
#define DEV_SERIAL 0x7

#endif /* __LUNAIX_DEVICE_NUM_H */
