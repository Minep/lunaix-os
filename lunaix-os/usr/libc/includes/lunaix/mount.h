#ifndef __LUNALIBC_SYS_MOUNT_H
#define __LUNALIBC_SYS_MOUNT_H

#include <lunaix/types.h>

extern int mount(const char* source, const char* target, const char* fstype, int flags);

extern int unmount(const char* target);

static inline int umount(const char* target)
{
  return unmount(target);
}
#endif /* __LUNALIBC_MOUNT_H */
