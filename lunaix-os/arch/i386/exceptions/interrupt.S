#define __ASM__
#include <sys/hart.h>
#include <sys/abi.h>
#include <sys/interrupt.S.inc>

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
    .type interrupt_wrapper, @function
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

        pushl $0      // placeholder for depth accounting

        movl ics(%esp), %eax   /* 取出 %cs */
        andl $0x3, %eax          /* 判断 RPL */
        jz 1f

    /* crossing the user/kernel boundary */
        movw $KDATA_SEG, %ax
        movw %ax, %gs
        movw %ax, %fs
        movw %ax, %ds
        movw %ax, %es

        movl current_thread, %ebx
        movl iuesp(%esp), %eax

        # Save x87 context to user stack, rather than kernel's memory.
        # XXX what will happen if we triggered a page fault during fxsave?
        # FIXME I think we should defer this to scheduler, and pratice lazy save/load
        #       Doing this will also make it safe from nested interrupt due to potential 
        #       page fault when saving
        # FIXME Also, generalise it to any FPU context, without constraining it to x87.

        #andl $stack_alignment, %eax 
        #subl $512, %eax
        #fxsave (%eax)

        # 保存用户栈顶指针。因为我们允许同级中断的产生，所以需要该手段跟踪用户栈的地址。
        movl %eax, thread_ustack_top(%ebx)     # 存入__current->ustack_top

    /* kernel space same-level switch */
    1:
        movl %esp, %eax
        andl $stack_alignment, %esp
        subl $16, %esp
        movl %eax, (%esp)

        xorl %ebp, %ebp     # marks the boundary of stack walking
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

        # # FIXME x87 fpu context 
        # movl current_thread, %eax
        # movl thread_ustack_top(%eax), %eax
        # test %eax, %eax
        # jz 1f
        # fxrstor (%eax)

1:
        popl %eax   # discard struct hart_state::depth
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
        movl current_thread, %eax

        # nested intr: restore saved context
        popl thread_hstate(%eax)

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

    .type do_switch, @function
    .global do_switch
    do_switch:
        # Assumption: __current already hold the target process

        call proc_vmroot

        movl %eax, %ebx
        movl %cr3, %eax
        xorl %ebx, %eax         # avoid setting cr3 if just local thread switch.
        jz 1f

        movl %ebx, %cr3         

    1:
        # the address space could be changed. A temporary stack
        # is required to prevent corrupt existing stack
        movl $tmp_stack, %esp

        call signal_dispatch    # kernel/signal.c

        movl current_thread, %ebx
        test %eax, %eax         # do we have signal to handle?
        jz 1f

        /*
            将tss.esp0设置为上次调度前的esp值。
            当处理信号时，上下文信息是不会恢复的，而是保存在用户栈中，然后直接跳转进位于用户空间的sig_wrapper进行
            信号的处理。当用户自定义的信号处理函数返回时，sigreturn的系统调用才开始进行上下文的恢复（或者说是进行
            另一次调度。
            由于这中间没有进行地址空间的交换，所以第二次跳转使用的是同一个内核栈，而之前默认tss.esp0的值是永远指向最顶部
            这样一来就有可能会覆盖更早的上下文信息（比如嵌套的信号捕获函数）
        */
        movl thread_hstate(%ebx), %ecx      # __current->hstate
        movl %ecx, (tss_esp0_off + _tss)

        jmp handle_signal

    1:
        movl thread_hstate(%ebx), %eax
        jmp soft_iret

    .type handle_signal, @function 
    .global handle_signal
    handle_signal:
        # 注意1：任何对proc_sig的布局改动，都须及时的保证这里的一致性！
        # 注意2：handle_signal在调用之前，须确保proc_sig已经写入用户栈！
        # arg1 in %eax: addr of proc_sig structure in user stack
        movl psig_saved_hstate(%eax), %ebx      # %ebx = &proc_sig->saved_hstate

        pushl $UDATA_SEG
        pushl %eax              # esp

        movl iexecp(%ebx), %ebx
        pushl exeflags(%ebx)          # proc_sig->saved_hstate->execp->eflags
        
        pushl $UCODE_SEG        # cs
        pushl psig_sigact(%eax)           # %eip = proc_sig->sigact

        movw $UDATA_SEG, %cx    # switch data seg to user mode
        movw %cx, %es
        movw %cx, %ds
        movw %cx, %fs
        movw %cx, %gs

        iret