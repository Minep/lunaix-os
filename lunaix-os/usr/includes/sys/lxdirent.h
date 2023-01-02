#ifndef __LUNAIX_SYS_LXDIRENT_H
#define __LUNAIX_SYS_LXDIRENT_H

#include <sys/dirent_defs.h>

int
sys_readdir(int fd, struct lx_dirent* dirent);

#endif /* __LUNAIX_DIRENT_H */
