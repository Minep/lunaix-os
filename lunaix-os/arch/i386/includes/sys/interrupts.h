#ifndef __LUNAIX_INTERRUPTS_H
#define __LUNAIX_INTERRUPTS_H

#include "vectors.h"

#ifndef __ASM__
#include <lunaix/compiler.h>
#include <sys/cpu.h>

struct exec_param;

struct regcontext
{
    u32_t eax;
    u32_t ebx;
    u32_t ecx;
    u32_t edx;
    u32_t edi;
    u32_t ebp;
    u32_t esi;
    u32_t ds;
    u32_t es;
    u32_t fs;
    u32_t gs;
} compact;

struct pcontext
{
    unsigned int depth;
    struct regcontext registers;
    union
    {
        u32_t esp;
        volatile struct exec_param* execp;
    };
} compact;

struct exec_param
{
    struct pcontext* saved_prev_ctx;
    u32_t vector;
    u32_t err_code;
    u32_t eip;
    u32_t cs;
    u32_t eflags;
    u32_t esp;
    u32_t ss;
} compact;

#define saved_fp(isrm) ((isrm)->registers.ebp)
#define kernel_context(isrm) (!(((isrm)->execp->cs) & 0b11))

#endif

#endif /* __LUNAIX_INTERRUPTS_H */
