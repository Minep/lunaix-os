#ifndef __LUNAIX_UDEVICE_H
#define __LUNAIX_UDEVICE_H

#include "ioctl_defs.h"

struct dev_info
{
    unsigned int extra;

    struct
    {
        unsigned int meta;
        unsigned int device;
        unsigned int variant;
    } dev_id;

    struct
    {
        char* buf;
        unsigned int buf_len;
    } dev_name;
};

#endif /* __LUNAIX_UDEVICE_H */
