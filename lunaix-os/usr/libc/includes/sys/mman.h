#ifndef __LUNALIBC_SYS_MANN_H
#define __LUNALIBC_SYS_MANN_H

#include <lunaix/mman.h>
#include <sys/types.h>

void* mmap(void* addr, size_t length, int proct, int flags, int fd, off_t offset);

int munmap(void* addr, size_t length);

#endif /* __LUNALIBC_MANN_H */
