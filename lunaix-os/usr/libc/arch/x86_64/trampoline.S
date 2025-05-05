#include <lunaix/syscallid.h>

.section .text
    /*
    stack structure:

        ...saved_state  \
        void* sighand;   > struct siguctx
        void* sigact;   /
        int sig_num;    <--- %rsp
    */
    .global sigtrampoline
    sigtrampoline:
        movq %rsp, %rax
        andq $-16, %rsp
        pushq %rax
        
        movl (%rax), %edi      // signum
        xorq %rsi, %rsi        // siginfo = NULL
        leaq 4(%rax), %rdx     // (struct siguctx*)&sigact
        call sig_dohandling
        
        movq $__NR__lxsys_sigreturn, %rax
        popq %rbx
        int $33
    
    .global th_trampoline
    th_trampoline:
        movq (%rsp), %rax
        movq 8(%rsp), %rdi

        callq *%rax

        movq %rax, %rbx
        movq $__NR__lxsys_th_exit, %rax
        int $33