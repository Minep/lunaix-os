#define __USR_WRAPPER__
#include <lunaix/ld.h>

int
usr_pre_init(struct usr_exec_param* param)
{
    // TODO some inits before executing user program

    return 0;
}