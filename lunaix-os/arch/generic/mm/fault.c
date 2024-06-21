#include <lunaix/mm/fault.h>

bool
__arch_prepare_fault_context(struct fault_context* fault)
{
    return false;
}