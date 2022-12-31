#include <lunaix/mm/page.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/status.h>

#include <lunaix/spike.h>
#include <lunaix/syscall.h>

__DEFINE_LXSYSCALL1(int, sbrk, size_t, size)
{
    // TODO mem_remap to expand heap region
    return 0;
}

__DEFINE_LXSYSCALL1(void*, brk, void*, addr)
{
    // TODO mem_remap to expand heap region
    return 0;
}