#define __ASM__ 1
#include <sys/boot/multiboot.h>

#define MB_FLAGS    (MULTIBOOT_MEMORY_INFO | MULTIBOOT_PAGE_ALIGN)
#define KPG_SIZE    10*4096

.section .multiboot
    .long MULTIBOOT_MAGIC
    .long MB_FLAGS
    .long CHECKSUM(MB_FLAGS)

.section .boot.bss
    /* 根据System V ABI，栈地址必须16字节对齐 */
    /* 这里只是一个临时栈，在_hhk_init里面我们会初始化内核专用栈 */
    .align 16
        .skip 256, 0
    __boot_stack_top:

.section .boot.text
    .global start_
    .type start_, @function     /* Optional, this just give the 
                                 * linker more knowledge about the label 
                                 */
    start_: 
        cld
        # 确保屏蔽所有外中断，直到我们准备好PIC为止
        cli
        movl $__boot_stack_top, %esp

        subl $16, %esp

        movl %ebx, (%esp)
        call x86_init

        addl $16, %esp

        pushl $hhk_entry_
        ret