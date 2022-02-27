#ifndef __LUNAIX_CPU_H
#define __LUNAIX_CPU_H

#include <stdint.h>

typedef unsigned int reg32;
typedef unsigned short reg16;

typedef struct {
    reg32 eax;
    reg32 ebx;
    reg32 ecx;
    reg32 edx;
    reg32 edi;
    reg32 ebp;
    reg32 esi;
    reg32 esp;
    reg32 cs;
    reg32 eip;
} __attribute__((packed)) registers;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
static inline reg32 cpu_rcr0() {
    asm volatile ("mov %cr0, %eax");
}

static inline reg32 cpu_rcr2() {
    asm volatile ("mov %cr2, %eax");
}

static inline reg32 cpu_rcr3() {
    asm volatile ("mov %cr3, %eax");
}
#pragma GCC diagnostic pop

static inline void cpu_lcr0(reg32 v) {
    asm volatile (
        "mov %0, %%cr0"
        :: "r"(v)
    );
}

static inline void cpu_lcr2(reg32 v) {
    asm volatile (
        "mov %0, %%cr2"
        :: "r"(v)
    );
}

static inline void cpu_lcr3(reg32 v) {
    asm volatile (
        "mov %0, %%cr3"
        :: "r"(v)
    );
}

static inline void cpu_invplg(void* va) {
    __asm__("invlpg (%0)" ::"r"((uintptr_t)va) : "memory");
}

static inline void cpu_invtlb() {
    reg32 interm;
    __asm__(
        "movl %%cr3, %0\n"
        "movl %0, %%cr3"
        :"=r"(interm)
        :"r"(interm)
    );
}

#endif