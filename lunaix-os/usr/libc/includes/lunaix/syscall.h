#ifndef __LUNAIX_OSDEPS_SYSCALL_H
#define __LUNAIX_OSDEPS_SYSCALL_H

#include <lunaix/syscallid.h>

#define variadic            __attribute__((noinline))
#define va_list_addr(arg)   ((unsigned long)&(arg) + sizeof(arg))

extern unsigned long __attribute__((regparm(0)))
do_lunaix_syscall(unsigned long call_id, ...);

#endif /* __LUNAIX_OSDEPS_SYSCALL_H */
