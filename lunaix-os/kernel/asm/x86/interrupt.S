#define __ASM__
#include <arch/x86/interrupts.h>
#include <arch/x86/i386_asm.h>
#include <lunaix/syscall.h>
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
        /*
         Stack layout (layout of struct isr_param)
    msa:   [ss]             > 76
           [esp]            > 72
           eflags           > 68
           cs               > 64
           eip              > 60
           err_code         > 56
           vector           > offset = 52
           [saved_prev_ctx] > offset = 0
           ---
           esp
           gs
           fs
           es
           ds         > offset = 7 * 4 = 28 + 4
           esi
           ebp
           edi
           edx
           ecx
           ebx
           eax
    lsa:   depth       > offset = 0

            las: Least Significant Address
            msa: Most Significant Address
        */
        cld

        subl $52, %esp
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
        movl 8(%eax), %eax
        incl %eax
        pushl %eax          # nested intr: current depth

        movl 116(%esp), %eax   /* 取出 %cs */
        andl $0x3, %eax          /* 判断 RPL */
        jz 1f

        movw $KDATA_SEG, %ax    /* 如果从用户模式转来，则切换至内核数据段 */
        movw %ax, %gs
        movw %ax, %fs
        movw %ax, %ds
        movw %ax, %es

        movl __current, %eax

        # FIXME: Save x87 context to user stack, rather than kernel's memory.
        # 保存x87FPU的状态
        movl 68(%eax), %ebx
        fxsave (%ebx)

        # 保存用户栈顶指针。因为我们允许同级中断的产生，所以需要该手段跟踪用户栈的地址。
        movl 124(%esp), %ebx     # 取出esp
        movl %ebx, 60(%eax)     # 存入__current->ustack_top

    1:
        movl %esp, %eax
        andl $0xfffffff0, %esp
        subl $16, %esp
        movl %eax, (%esp)

        call intr_handler

        movl (%esp), %eax

    .global soft_iret
    soft_iret:
        movl %eax, %esp

#ifdef __ASM_INTR_DIAGNOSIS
        movl %eax, (debug_resv + 8)
        movl 48(%esp), %eax
        movl 60(%eax), %eax
        movl %eax, (debug_resv + 4) # eip
#endif
        movl __current, %eax
        movl 68(%eax), %eax
        
        test %eax, %eax     # do we have stored x87 context?
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
        popl 8(%eax)       # depth
        popl 12(%eax)      # eax
        popl 16(%eax)      # ebx
        popl 20(%eax)      # ecx
        popl 24(%eax)      # edx
        popl 28(%eax)      # edi
        popl 32(%eax)      # ebp
        popl 36(%eax)      # esi
        popl 40(%eax)      # ds
        popl 44(%eax)      # es
        popl 48(%eax)      # fs
        popl 52(%eax)      # gs
        popl 56(%eax)      # esp

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
        movl %eax, (_tss + 4)
        movl tmp_store, %eax
        iret

    .global switch_to
    switch_to:
        # 约定
        # arg1: 目标进程PCB地址 (next

        popl %ebx               # next
        movl __current, %eax    
        movl 64(%eax), %ecx     # __current->pagetable
        movl 64(%ebx), %eax     # next->pagetable
        
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
        jmp handle_signal
    1:
        leal 8(%ebx), %eax
        jmp soft_iret

    .global handle_signal
    handle_signal:
        # 注意1：任何对proc_sig的布局改动，都须及时的保证这里的一致性！
        # 注意2：handle_signal在调用之前，须确保proc_sig已经写入用户栈！
        # arg1 in %eax: addr of proc_sig structure in user stack
        leal 12(%eax), %ebx      # %ebx = &proc_sig->prev_context

        pushl $UDATA_SEG        # proc_sig->prev_context.proc_regs.ss
        pushl %eax              # esp

        movl 48(%ebx), %ebx
        pushl 68(%ebx)          # proc_sig->prev_context.proc_regs.execp->eflags
        
        pushl $UCODE_SEG        # cs
        pushl 4(%eax)           # %eip = proc_sig->sigact

        movw $UDATA_SEG, %cx    # switch data seg to user mode
        movw %cx, %es
        movw %cx, %ds
        movw %cx, %fs
        movw %cx, %gs

        iret