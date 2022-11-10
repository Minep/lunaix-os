#define __ASM__
#include <arch/x86/interrupts.h>
#include <lunaix/common.h>
#include <lunaix/syscall.h>
#define __ASM_INTR_DIAGNOSIS

#ifdef __ASM_INTR_DIAGNOSIS
.section .bss
    .global debug_resv
    debug_resv:
        .skip 16
#endif

.section .bss
    .align 16
    lo_tmp_stack:
        .skip 128
    tmp_stack:

.section .text
    .global interrupt_wrapper
    interrupt_wrapper:
        /*
         Stack layout (layout of struct isr_param)
    msa:   [ss]
           [esp]
           eflags     > offset = 48 + 16 = 64
           cs
           eip
           err_code   
           vector     > offset = 28 + 16 + 4 = 48
           esp
           gs
           fs
           es
           ds         > offset = 7 * 4 = 28
           esi
           ebp
           edi
           edx
           ecx
           ebx
    lsa:   eax        > offset = 0

            las: Least Significant Address
            msa: Most Significant Address
        */
        cld
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

        movl 60(%esp), %eax   /* 取出 %cs */
        andl $0x3, %eax          /* 判断 RPL */
        jz 1f

        movw $KDATA_SEG, %ax    /* 如果从用户模式转来，则切换至内核数据段 */
        movw %ax, %gs
        movw %ax, %fs
        movw %ax, %ds
        movw %ax, %es

        # 保存用户栈顶指针。这是因为我们允许系统调用内进行上下文切换，而这样一来，我们就失去了用户栈的信息，
        # 这样一来，就无法设置信号上下文。这主要是为了实现了pause()而做的准备
        movl (__current), %eax

        # 保存x87FPU的状态
        movl 92(%eax), %ebx
        fxsave (%ebx)

        movl 68(%esp), %ebx     # 取出esp
        movl %ebx, 84(%eax)     # 存入__current->ustack_top

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
        movl 56(%esp), %eax
        movl %eax, (debug_resv + 4)
#endif
        movl (__current), %eax
        movl 92(%eax), %eax
        
        test %eax, %eax     # do we have stored x87 context?
        jz 1f 
        fxrstor (%eax)      
1:
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

        addl $8, %esp

        pushl %eax
#ifdef __ASM_INTR_DIAGNOSIS
        movl 4(%esp), %eax
        movl %eax, debug_resv
#endif
        # 处理TSS.ESP的一些边界条件。如果是正常iret（即从内核模式*优雅地*退出）
        # 那么TSS.ESP0应该为iret进行弹栈后，%esp的值。
        # 所以这里的边界条件是：如返回用户模式，iret会额外弹出8个字节（ss,esp）
        movl 8(%esp), %eax
        andl $3, %eax
        setnz %al
        shll $3, %eax
        addl $16, %eax
        addl %esp, %eax
        movl %eax, (_tss + 4)
        popl %eax
        iret

    .global switch_to
    switch_to:
        # 约定
        # arg1: 目标进程PCB地址 (next

        popl %ebx               # next
        movl __current, %eax    
        movl 88(%eax), %ecx     # __current->pagetable
        movl 88(%ebx), %eax     # next->pagetable
        
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
        leal 8(%eax), %ebx      # arg1 in %eax: addr of proc_sig structure in user stack

        pushl $UDATA_SEG        # proc_sig->prev_context.proc_regs.ss
        pushl %eax              # esp
        pushl 64(%ebx)          # proc_sig->prev_context.proc_regs.eflags
        pushl $UCODE_SEG        # cs
        pushl $sig_wrapper      # eip for sig wrapper

        movw $UDATA_SEG, %cx    # switch data seg to user mode
        movw %cx, %es
        movw %cx, %ds
        movw %cx, %fs
        movw %cx, %gs

        iret  

.section .usrtext
    sig_wrapper:                # in user mode
        movl %esp, %eax
        and $0xfffffff0, %esp
        subl $8, %esp
        pushl %eax              # Addr to proc_sig structure 
        pushl 4(%eax)           # proc_sig->sig_num     ---- 16 bytes aligned

        call (%eax)             # invoke signal handler

        # invoke the sigreturn syscall to exit the signal wrapper
        movl $__SYSCALL_sigreturn, %eax
        movl 4(%esp), %ebx
        int $LUNAIX_SYS_CALL    

        ud2                     # never reach!