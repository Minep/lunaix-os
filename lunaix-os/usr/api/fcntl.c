#include <lunaix/syscall.h>
#include <usr/fcntl.h>

__LXSYSCALL2(int, open, const char*, path, int, options)
