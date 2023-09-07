#include "syscall.h"
#include <fcntl.h>

__LXSYSCALL2(int, open, const char*, path, int, options)

__LXSYSCALL2(int, fstat, int, fd, struct file_stat*, stat)
