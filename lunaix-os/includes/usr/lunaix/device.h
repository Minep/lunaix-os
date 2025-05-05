#ifndef _LUNAIX_UHDR_UDEVICE_H
#define _LUNAIX_UHDR_UDEVICE_H

#include "ioctl.h"

struct dev_info
{
    unsigned int extra;

    struct
    {
        unsigned int group;
        unsigned int unique;
    } dev_id;

    struct
    {
        char* buf;
        unsigned int buf_len;
    } dev_name;
};

#endif /* _LUNAIX_UHDR_UDEVICE_H */
