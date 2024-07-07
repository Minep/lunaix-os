#ifndef __LUNAIX_SYS_MANN_H
#define __LUNAIX_SYS_MANN_H

#include <stddef.h>
#include <lunaix/mann_flags.h>
#include <lunaix/types.h>

void* mmap(void* addr, size_t length, int proct, int flags, int fd, off_t offset);

int munmap(void* addr, size_t length);

#endif /* __LUNAIX_MANN_H */
