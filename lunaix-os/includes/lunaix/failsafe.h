#ifndef __LUNAIX_FAILSAFE_H
#define __LUNAIX_FAILSAFE_H

#include <sys/failsafe.h>

void
do_failsafe_unrecoverable(ptr_t frame_link, ptr_t stack_link);

#endif /* __LUNAIX_FAILSAFE_H */
