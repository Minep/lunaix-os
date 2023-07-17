#include "syscall.h"

__LXSYSCALL4(int,
             mount,
             const char*,
             source,
             const char*,
             target,
             const char*,
             fstype,
             int,
             options)

__LXSYSCALL1(int, unmount, const char*, target)