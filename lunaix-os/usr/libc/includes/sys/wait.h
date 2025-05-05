#ifndef __LUNAIX_WAIT_H
#define __LUNAIX_WAIT_H

#include <lunaix/wait.h>
#include <sys/types.h>

pid_t
wait(int* status);

pid_t
waitpid(pid_t pid, int* status, int flags);

#endif /* __LUNAIX_WAIT_H */
