#ifndef __LUNAIX_SYS_MANN_H
#define __LUNAIX_SYS_MANN_H

#include <sys/mann_flags.h>
#include <sys/types.h>

void*
mmap(void* addr, size_t length, int proct, int flags, int fd, off_t offset);

void
munmap(void* addr, size_t length);

#endif /* __LUNAIX_MANN_H */
