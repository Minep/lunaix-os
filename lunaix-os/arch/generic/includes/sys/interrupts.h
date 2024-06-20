#ifndef __LUNAIX_ARCH_INTERRUPTS_H
#define __LUNAIX_ARCH_INTERRUPTS_H

#ifndef __ASM__
#include <lunaix/compiler.h>
#include <sys/cpu.h>

struct exec_param;

struct regcontext
{
    
} compact;

struct pcontext
{
    
} compact;

struct exec_param
{
    struct pcontext* saved_prev_ctx;
} compact;

#define saved_fp(isrm) (void)
#define kernel_context(isrm) (void)

#endif

#endif /* __LUNAIX_ARCH_INTERRUPTS_H */
