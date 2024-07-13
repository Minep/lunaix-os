#include <lunaix/syscallid.h>

.section .text
    .global sigtrampoline
    /*
    stack structure:

        ...saved_state  \
        void* sighand;   > struct siguctx
        void* sigact;   /
        int sig_num;    <--- %rsp
    */
    sigtrampoline:
        movq %rsp, %rax
        andq $-16, %rsp
        pushq %rax
        
        movl (%rax), %edi      // signum
        xorq %rsi, %rsi        // siginfo = NULL
        leaq 4(%rax), %rdx     // (struct siguctx*)&sigact
        call sig_dohandling
        
        movq $__SYSCALL_sigreturn, %rax
        popq %rbx
        int $33