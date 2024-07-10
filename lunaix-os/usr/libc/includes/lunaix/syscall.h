#ifndef __LUNAIX_OSDEPS_SYSCALL_H
#define __LUNAIX_OSDEPS_SYSCALL_H

#include <lunaix/syscallid.h>

extern unsigned long
do_lunaix_syscall(unsigned long call_id, ...);

#endif /* __LUNAIX_OSDEPS_SYSCALL_H */
