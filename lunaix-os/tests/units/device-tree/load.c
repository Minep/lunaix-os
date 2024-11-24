#include "common.h"

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#define MAX_DTB_SZ (2 * 1024 * 1024)

#ifndef TEST_DTBFILE
#error "no test dtb file given"
#endif

extern void init_export___init_devtree();

bool
my_load_dtb()
{
    int fd = open(TEST_DTBFILE, O_RDONLY);
    if (fd == -1) {
        printf("fail to open: %s, %s\n", TEST_DTBFILE, strerror(errno));
        return false;
    }

    ptr_t dtb = (ptr_t)mmap(0, 
                            MAX_DTB_SZ, 
                            PROT_READ | PROT_WRITE | MAP_POPULATE, 
                            MAP_PRIVATE, fd, 0);
    
    if (dtb == (ptr_t)-1) {
        printf("fail to map dtb: %s\n", strerror(errno));
        return 1;
    }

    close(fd);
    init_export___init_devtree();

    return dt_load(dtb);
}