#ifndef __LUNAIX_SYS_MOUNT_H
#define __LUNAIX_SYS_MOUNT_H

#include <usr/sys/types.h>

int
mount(const char* source, const char* target, const char* fstype, int flags);

int
unmount(const char* target);

static inline int
umount(const char* target)
{
    return unmount(target);
}
#endif /* __LUNAIX_MOUNT_H */
