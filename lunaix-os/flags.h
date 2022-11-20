#ifndef __LUNAIX_FLAGS_H
#define __LUNAIX_FLAGS_H

#ifdef __ARCH_IA32
#define PLATFORM_TARGET "x86_32"
#else
#define PLATFORM_TARGET "unknown"
#endif

#define LUNAIX_VER "0.0.1-dev"

/*
    Uncomment below to force LunaixOS use kernel page table when context switch
   to kernel space NOTE: This will make the kernel global.
*/
// #define USE_KERNEL_PG

/*
    Uncomment below to disable all assertion
*/
// #define __LUNAIXOS_NASSERT__

#endif /* __LUNAIX_FLAGS_H */
