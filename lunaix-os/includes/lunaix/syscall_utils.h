#ifndef __LUNAIX_SYSCALL_UTILS_H
#define __LUNAIX_SYSCALL_UTILS_H

#include <lunaix/process.h>
#include <lunaix/syscall.h>

#define DO_STATUS(errno) SYSCALL_ESTATUS(syscall_result(errno))
#define DO_STATUS_OR_RETURN(errno) ({ errno < 0 ? DO_STATUS(errno) : errno; })

#endif /* __LUNAIX_SYSCALL_UTILS_H */
