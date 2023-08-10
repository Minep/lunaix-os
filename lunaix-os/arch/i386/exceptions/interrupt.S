#define __ASM__
#include <arch/i386/interrupts.h>
#include <arch/i386/i386_asm.h>
#include <arch/i386/tss.h>
#include <lunaix/syscall.h>
#include <arch/i386/interrupt.S.inc>

#define __ASM_INTR_DIAGNOSIS

#ifdef __ASM_INTR_DIAGNOSIS
.section .bss
    .global debug_resv
    debug_resv:
        .skip 16
    tmp_store:
        .skip 4
#endif

.section .bss
    .align 16
    lo_tmp_stack:
        .skip 256
    tmp_stack:

/*
    This perhaps the ugliest part in the project. 
    It contains code to handle arbitrary depth of 
    nested interrupt and all those corner cases and 
    nasty gotchas.

    Be aware the twists, offsets and hidden dependencies!

*/

.section .text
    .global interrupt_wrapper
    interrupt_wrapper:
        cld

        subl $4, %esp
        pushl %esp

        subl $16, %esp
        movw %gs, 12(%esp)
        movw %fs,  8(%esp)
        movw %es,  4(%esp)
        movw %ds,   (%esp)

        pushl %esi
        pushl %ebp
        pushl %edi
        pushl %edx
        pushl %ecx
        pushl %ebx
        pushl %eax

        movl __current, %eax
        movl proc_intr_ctx(%eax), %eax
        incl %eax
        pushl %eax          # nested intr: current depth

        movl ics(%esp), %eax   /* 取出 %cs */
        andl $0x3, %eax          /* 判断 RPL */
        jz 1f

    /* crossing the user/kernel boundary */
        movw $KDATA_SEG, %ax
        movw %ax, %gs
        movw %ax, %fs
        movw %ax, %ds
        movw %ax, %es

        movl __current, %ebx

        # Save x87 context to user stack, rather than kernel's memory.
        # XXX what will happen if we triggered a page fault during fxsave?
        # FIXME can we remove this overhead?
        movl iuesp(%esp), %eax
        andl $stack_alignment, %eax 
        subl $512, %eax
        fxsave (%eax)

        # 保存用户栈顶指针。因为我们允许同级中断的产生，所以需要该手段跟踪用户栈的地址。
        movl %eax, proc_ustack_top(%ebx)     # 存入__current->ustack_top

    /* kernel space same-level switch */
    1:
        movl %esp, %eax
        andl $stack_alignment, %esp
        subl $16, %esp
        movl %eax, (%esp)

        call intr_handler

        movl (%esp), %eax

    .global soft_iret
    soft_iret:
        movl %eax, %esp

#ifdef __ASM_INTR_DIAGNOSIS
        movl %eax, (debug_resv + 8)
        movl iesp(%esp), %eax
        movl exeip(%eax), %eax
        movl %eax, (debug_resv + 4) # eip
#endif

        movl ics(%esp), %eax
        andl $3, %eax
        jz 1f 

        movl __current, %eax
        movl proc_ustack_top(%eax), %eax
        test %eax, %eax
        jz 1f
        fxrstor (%eax)

1:
        popl %eax   # discard isr_param::depth
        popl %eax
        popl %ebx
        popl %ecx
        popl %edx
        popl %edi
        popl %ebp
        popl %esi
        
        movw   (%esp), %ds
        movw  4(%esp), %es
        movw  8(%esp), %fs
        movw 12(%esp), %gs

        movl 16(%esp), %esp

        movl %eax, tmp_store
        movl __current, %eax

        # nested intr: restore saved context
        popl proc_intr_ctx(%eax)

        addl $8, %esp

#ifdef __ASM_INTR_DIAGNOSIS
        movl (%esp), %eax
        movl %eax, debug_resv
#endif
        # 处理TSS.ESP的一些边界条件。如果是正常iret（即从内核模式*优雅地*退出）
        # 那么TSS.ESP0应该为iret进行弹栈后，%esp的值。
        # 所以这里的边界条件是：如返回用户模式，iret会额外弹出8个字节（ss,esp）
        movl 4(%esp), %eax
        andl $3, %eax
        setnz %al
        shll $3, %eax
        addl $12, %eax
        addl %esp, %eax
        movl %eax, (_tss + tss_esp0_off)
        movl tmp_store, %eax

        iret

    .global switch_to
    switch_to:
        # 约定
        # arg1: 目标进程PCB地址 (next

        popl %ebx               # next
        movl __current, %eax    
        movl proc_page_table(%eax), %ecx     # __current->pagetable
        movl proc_page_table(%ebx), %eax     # next->pagetable
        
        cmpl %ecx, %eax         # if(next->pagtable != __current->pagetable) {
        jz 1f
        movl %eax, %cr3         #   cpu_lcr3(next->pagetable)
                                # }
    1:
        movl %ebx, __current    # __current = next

        # 我们已经处在了新的地址空间，为了避免影响其先前的栈布局
        # 需要使用一个临时的栈空间
        movl $tmp_stack, %esp

        call signal_dispatch    # kernel/signal.c

        test %eax, %eax         # do we have signal to handle?
        jz 1f

        # 更新 tss
        movl proc_intr_ctx(%ebx), %ecx      # __current->intr_ctx
        movl %ecx, (tss_esp0_off + _tss)

        jmp handle_signal

    1:
        movl proc_intr_ctx(%ebx), %eax
        jmp soft_iret

    .global handle_signal
    handle_signal:
        # 注意1：任何对proc_sig的布局改动，都须及时的保证这里的一致性！
        # 注意2：handle_signal在调用之前，须确保proc_sig已经写入用户栈！
        # arg1 in %eax: addr of proc_sig structure in user stack
        movl psig_saved_ictx(%eax), %ebx      # %ebx = &proc_sig->saved_ictx

        pushl $UDATA_SEG
        pushl %eax              # esp

        movl iexecp(%ebx), %ebx
        pushl exeflags(%ebx)          # proc_sig->saved_ictx->execp->eflags
        
        pushl $UCODE_SEG        # cs
        pushl psig_sigact(%eax)           # %eip = proc_sig->sigact

        movw $UDATA_SEG, %cx    # switch data seg to user mode
        movw %cx, %es
        movw %cx, %ds
        movw %cx, %fs
        movw %cx, %gs

        iret