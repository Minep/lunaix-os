#define __ASM__
#include <lunaix/syscall.h>
#include "sys/interrupt64.S.inc"

.section .data
    /*
        注意，这里的顺序非常重要。每个系统调用在这个地址表里的索引等于其调用号。
    */
    syscall_table:
        1:
        .long 0
        .long __lxsys_fork          /* 1 */
        .long __lxsys_yield
        .long __lxsys_sbrk
        .long __lxsys_brk
        .long __lxsys_getpid        /* 5 */
        .long __lxsys_getppid
        .long __lxsys_sleep
        .long __lxsys_exit
        .long __lxsys_wait          
        .long __lxsys_waitpid       /* 10 */
        .long __lxsys_sigreturn
        .long __lxsys_sigprocmask
        .long __lxsys_sys_sigaction
        .long __lxsys_pause
        .long __lxsys_kill          /* 15 */
        .long __lxsys_alarm
        .long __lxsys_sigpending
        .long __lxsys_sigsuspend
        .long __lxsys_open
        .long __lxsys_close         /* 20 */
        .long __lxsys_read
        .long __lxsys_write
        .long __lxsys_sys_readdir
        .long __lxsys_mkdir
        .long __lxsys_lseek         /* 25 */
        .long __lxsys_geterrno
        .long __lxsys_readlink
        .long __lxsys_readlinkat
        .long __lxsys_rmdir
        .long __lxsys_unlink        /* 30 */
        .long __lxsys_unlinkat
        .long __lxsys_link
        .long __lxsys_fsync
        .long __lxsys_dup
        .long __lxsys_dup2          /* 35 */
        .long __lxsys_realpathat
        .long __lxsys_symlink
        .long __lxsys_chdir
        .long __lxsys_fchdir
        .long __lxsys_getcwd        /* 40 */
        .long __lxsys_rename
        .long __lxsys_mount
        .long __lxsys_unmount
        .long __lxsys_getxattr
        .long __lxsys_setxattr      /* 45 */
        .long __lxsys_fgetxattr
        .long __lxsys_fsetxattr
        .long __lxsys_ioctl
        .long __lxsys_getpgid
        .long __lxsys_setpgid       /* 50 */
        .long __lxsys_syslog
        .long __lxsys_sys_mmap
        .long __lxsys_munmap
        .long __lxsys_execve
        .long __lxsys_fstat         /* 55 */
        .long __lxsys_pollctl
        .long __lxsys_th_create
        .long __lxsys_th_self
        .long __lxsys_th_exit
        .long __lxsys_th_join       /* 60 */
        .long __lxsys_th_kill
        .long __lxsys_th_detach
        .long __lxsys_th_sigmask
        2:
        .rept __SYSCALL_MAX - (2b - 1b) / 8
            .long 0
        .endr


.section .text
    .type syscall_hndlr, @function
    .global syscall_hndlr
    syscall_hndlr:
        pushq %rbp
        movq %rsp, %rbp
        movq 16(%rsp), %rbx          // struct hart_state*

        leaq irax(%rbx), %rbx
        movq (%rbx), %rax          /* rax: call code as well as the return value from syscall */
        cmpq $__SYSCALL_MAX, %rax
        jae 2f

        shll $3, %rax               // %rax * 8
        addl $syscall_table, %rax
        cmpq $0, (%rax)
        jne 1f
    2:    
        neg   %rax
        movq %rbp, %rsp
        popl  %rbp
        ret

    1:
        pushq    %rbx

        movq  28(%rbx), %r8     /* rsi -> r8  #5 arg */
        movq  20(%rbx), %rcx    /* rdi -> rcx #4 arg */
        movq  16(%rbx), %rdx    /* rdx -> rdx #3 arg */
        movq  12(%rbx), %rsi    /* rcx -> rsi #2 arg */
        movq   8(%rbx), %rdi    /* rbx -> rdi #1 arg */
        
        call   *(%rax)

        addl       $40, %rsp      /* remove the parameters from stack */
        
        popq     %rbx
        movq     %rax, (%rbx)    /* save the return value */
        
        movq     %rbp,  %rsp
        popq     %rbp
        
        ret