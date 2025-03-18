#ifndef __LUNAIX_USERCAPS_H
#define __LUNAIX_USERCAPS_H

typedef unsigned long caps_t;

/*
 *  Definition of user capabilities (withdrawn draft POSIX.1e) 
 */

#define CAP_SETUID          0
#define CAP_SETGID          1
#define CAP_CHOWN           2
#define CAP_DAC_OVERRIDE    3
#define CAP_FSETID          4
#define CAP_KILL            5
#define CAP_SYS_CHROOT      6
#define CAP_SYS_TIM         7

#endif /* __LUNAIX_USERCAPS_H */
