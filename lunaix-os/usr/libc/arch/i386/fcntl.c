#include "syscall.h"
#include <fcntl.h>

__LXSYSCALL2(int, open, const char*, path, int, options)
