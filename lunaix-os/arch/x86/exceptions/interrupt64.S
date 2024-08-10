#define __ASM__
#include <sys/hart.h>
#include <sys/abi.h>
#include <sys/interrupt64.S.inc>

#include <lunaix/syscall.h>

.section .bss
    .align 16
    tmp_store:
        .skip 8
    lo_tmp_stack:
        .skip 1024
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

        subq $8, %rsp
        pushq %rsp

        pushq %r15
        pushq %r14
        pushq %r13
        pushq %r12
        pushq %r11
        pushq %r10
        pushq %r9
        pushq %r8

        pushq %rsi
        pushq %rbp
        pushq %rdi
        pushq %rdx
        pushq %rcx
        pushq %rbx
        pushq %rax

        pushq $0      // placeholder for depth accounting

        movq ics(%rsp), %rax   /* 取出 %cs */
        andq $0x3, %rax          /* 判断 RPL */
        jz 1f

    /* crossing the user/kernel boundary */
        # x86_64 ignore {d,e}s, Lunaix does not use {f,g}s

        movq current_thread, %rbx
        movq iursp(%rsp), %rax

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
        movq %rax, thread_ustack_top(%rbx)     # 存入__current->ustack_top

    /* kernel space same-level switch */
    1:
        movq %rsp, %rax
        andq $stack_alignment, %rsp
                
        movq %rax, %rdi

        xorq %rbp, %rbp     # marks the boundary of stack walking
        call intr_handler

    .global soft_iret
    soft_iret:
        movq %rax, %rsp

        movq ics(%rsp), %rax
        andq $3, %rax
        jz 1f 

        # # FIXME x87 fpu context 
        # movl current_thread, %eax
        # movl thread_ustack_top(%eax), %eax
        # test %eax, %eax
        # jz 1f
        # fxrstor (%eax)

1:
        popq %rax   # discard struct hart_state::depth

        popq %rax
        popq %rbx
        popq %rcx
        popq %rdx
        popq %rdi
        popq %rbp
        popq %rsi
        
        popq %r8
        popq %r9
        popq %r10
        popq %r11
        popq %r12
        popq %r13
        popq %r14
        popq %r15

        popq %rsp

        movq %rax, tmp_store
        movq current_thread, %rax

        # nested intr: restore saved context
        popq thread_hstate(%rax)

        movq tmp_store, %rax

        addq $16, %rsp  # skip: err_code and vector

        # calculate stack after iretq
        # Thank god they finally make things consistent in x86_64
        addq $40, %rsp
        movq %rsp, (_tss + rsp_0)
        subq $40, %rsp

        iretq

    .type do_switch, @function
    .global do_switch
    do_switch:
        # Assumption: __current already hold the target process

        call proc_vmroot

        movq %rax, %rbx
        movq %cr3, %rax
        xorq %rbx, %rax         # avoid setting cr3 if just local thread switch.
        jz 1f

        movq %rbx, %cr3         

    1:
        # the address space could be changed. A temporary stack
        # is required to prevent corrupt existing stack
        movq $tmp_stack, %rsp

        call switch_signposting    # kernel/process/switch.c

        movq current_thread, %rbx
        test %rax, %rax         # do we have signal to handle?
        jz 1f

        /*
            将tss.esp0设置为上次调度前的esp值。
            当处理信号时，上下文信息是不会恢复的，而是保存在用户栈中，然后直接跳转进位于用户空间的sig_wrapper进行
            信号的处理。当用户自定义的信号处理函数返回时，sigreturn的系统调用才开始进行上下文的恢复（或者说是进行
            另一次调度。
            由于这中间没有进行地址空间的交换，所以第二次跳转使用的是同一个内核栈，而之前默认tss.esp0的值是永远指向最顶部
            这样一来就有可能会覆盖更早的上下文信息（比如嵌套的信号捕获函数）
        */
        pushq %rax
        call update_tss

        popq %rax
        jmp handle_signal

    1:
        movq thread_hstate(%rbx), %rax
        jmp soft_iret

    .type handle_signal, @function 
    .global handle_signal
    handle_signal:
        # 注意1：任何对proc_sig的布局改动，都须及时的保证这里的一致性！
        # 注意2：handle_signal在调用之前，须确保proc_sig已经写入用户栈！
        # arg1 in %rax: addr of proc_sig structure in user stack
        movq psig_saved_hstate(%rax), %rbx      # %rbx = &proc_sig->saved_hstate

        pushq $UDATA_SEG
        pushq %rax              # esp

        movq iexecp(%rbx), %rbx
        pushq exrflags(%rbx)          # proc_sig->saved_hstate->execp->eflags
        
        pushq $UCODE_SEG        # cs
        pushq psig_sigact(%rax)           # %rip = proc_sig->sigact

        iretq