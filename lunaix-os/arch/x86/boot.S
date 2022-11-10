#define __ASM__ 1
#include <arch/x86/boot/multiboot.h>

#define MB_FLAGS    MULTIBOOT_MEMORY_INFO | MULTIBOOT_PAGE_ALIGN
#define KPG_SIZE    10*4096

.section .multiboot
    .long MULTIBOOT_MAGIC
    .long MB_FLAGS
    .long CHECKSUM(MB_FLAGS)

.section .bss
    .global mb_info
    .align 16
    /* 为Multiboot info struct 预留空间 */
    mb_info:
        .skip 4096
    /* 根据System V ABI，栈地址必须16字节对齐 */
    /* 这里只是一个临时栈，在_hhk_init里面我们会初始化内核专用栈 */
    stack_bottom:
        .skip 4096, 0
    __stack_top:


/* 
    1 page directory, 
    9 page tables:
        1. Mapping reserved area and hhk_init
        2-9. Remapping the kernels
*/

.section .kpg
    .global _k_ptd
    _k_ptd:
        .skip KPG_SIZE, 0


.section .hhk_init
    .global start_
    .type start_, @function     /* Optional, this just give the 
                                 * linker more knowledge about the label 
                                 */
    start_: 
        cld
        # 确保屏蔽所有外中断，直到我们准备好PIC为止
        cli
        movl $__stack_top, %esp

        subl $16, %esp

        /* 
            将咱们的 multiboot_info 挪个地儿，就是上述预留的空间里
            而后在_hhk_init里，我们会对所有的高半核初始化代码（arch/x86下的所有）进行Identity map
            这样，我们能够保证当分页与虚拟地址开启后，我们的内核能够访问到multiboot info table
         */

        movl $mb_info, 4(%esp)
        movl %ebx, (%esp)
        call _save_multiboot_info

        /*
            _hhk_init用来初始化我们高半核：
                1. 初始化最简单的PD与PT（重新映射我们的内核至3GiB处，以及对相应的地方进行Identity Map）
        */

        movl $(KPG_SIZE), 4(%esp)
        movl $(_k_ptd - 0xC0000000), (%esp)    /* PTD物理地址 */
        call _hhk_init

        /*
            基本的映射定义好了，我们可以放心的打开分页了
            我们只需要把PTD的基地址加载进CR3就好了。
        */

        /* 加载PTD基地址（物理地址） */
        movl (%esp), %eax
        andl $0xfffff000, %eax      # 有点多余，但写上还算明白一点
        movl %eax, %cr3

        movl %cr0, %eax
        orl $0x80010000, %eax   /* 开启分页与地址转换 (CR0.PG=1, CR0.WP=1) */
        andl $0xfffffffb, %eax
        orl $0x2, %eax          /* 启用x87 FPU (CR0.MP=1, CR0.EM=0) */
        movl %eax, %cr0

        movl %cr4, %eax
        orl $0x600, %eax
        movl %eax, %cr4         /* CR4.OSFXSR=1, CR4.OSXMMEXCPT=1 */
                                /* x87 FPU 已配置 */

        addl $16, %esp

        /* 进入高半核！ */
        pushl $hhk_entry_
        ret