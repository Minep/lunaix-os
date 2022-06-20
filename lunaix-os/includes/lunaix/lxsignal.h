#ifndef __LUNAIX_LXSIGNAL_H
#define __LUNAIX_LXSIGNAL_H

#include <lunaix/types.h>

int
signal_send(pid_t pid, int signum);

#endif /* __LUNAIX_LXSIGNAL_H */
