#define __ASM__
#include <lunaix/syscall.h>
#include "sys/interrupt64.S.inc"

.section .data
    /*
        注意，这里的顺序非常重要。每个系统调用在这个地址表里的索引等于其调用号。
    */
    syscall_table:
        1:
        .8byte 0
        .8byte __lxsys_fork          /* 1 */
        .8byte __lxsys_yield
        .8byte __lxsys_sbrk
        .8byte __lxsys_brk
        .8byte __lxsys_getpid        /* 5 */
        .8byte __lxsys_getppid
        .8byte __lxsys_sleep
        .8byte __lxsys_exit
        .8byte __lxsys_wait          
        .8byte __lxsys_waitpid       /* 10 */
        .8byte __lxsys_sigreturn
        .8byte __lxsys_sigprocmask
        .8byte __lxsys_sys_sigaction
        .8byte __lxsys_pause
        .8byte __lxsys_kill          /* 15 */
        .8byte __lxsys_alarm
        .8byte __lxsys_sigpending
        .8byte __lxsys_sigsuspend
        .8byte __lxsys_open
        .8byte __lxsys_close         /* 20 */
        .8byte __lxsys_read
        .8byte __lxsys_write
        .8byte __lxsys_sys_readdir
        .8byte __lxsys_mkdir
        .8byte __lxsys_lseek         /* 25 */
        .8byte __lxsys_geterrno
        .8byte __lxsys_readlink
        .8byte __lxsys_readlinkat
        .8byte __lxsys_rmdir
        .8byte __lxsys_unlink        /* 30 */
        .8byte __lxsys_unlinkat
        .8byte __lxsys_link
        .8byte __lxsys_fsync
        .8byte __lxsys_dup
        .8byte __lxsys_dup2          /* 35 */
        .8byte __lxsys_realpathat
        .8byte __lxsys_symlink
        .8byte __lxsys_chdir
        .8byte __lxsys_fchdir
        .8byte __lxsys_getcwd        /* 40 */
        .8byte __lxsys_rename
        .8byte __lxsys_mount
        .8byte __lxsys_unmount
        .8byte __lxsys_getxattr
        .8byte __lxsys_setxattr      /* 45 */
        .8byte __lxsys_fgetxattr
        .8byte __lxsys_fsetxattr
        .8byte __lxsys_ioctl
        .8byte __lxsys_getpgid
        .8byte __lxsys_setpgid       /* 50 */
        .8byte __lxsys_syslog
        .8byte __lxsys_sys_mmap
        .8byte __lxsys_munmap
        .8byte __lxsys_execve
        .8byte __lxsys_fstat         /* 55 */
        .8byte __lxsys_pollctl
        .8byte __lxsys_th_create
        .8byte __lxsys_th_self
        .8byte __lxsys_th_exit
        .8byte __lxsys_th_join       /* 60 */
        .8byte __lxsys_th_kill
        .8byte __lxsys_th_detach
        .8byte __lxsys_th_sigmask
        2:
        .rept __SYSCALL_MAX - (2b - 1b) / 8
            .8byte 0
        .endr


.section .text
    .type syscall_hndlr, @function
    .global syscall_hndlr
    syscall_hndlr:
        pushq   %rbp
        movq    %rsp, %rbp
        movq    16(%rsp), %rbx          // struct hart_state*

        leaq    irax(%rbx), %rbx
        movq    (%rbx), %rax          /* rax: call code as well as the return value from syscall */
        cmpq    $__SYSCALL_MAX, %rax
        jae     2f

        shlq    $3, %rax               // %rax * 8
        movabsq $syscall_table, %r8
        addq    %r8, %rax
        cmpq    $0, (%rax)
        jne     1f
    2:    
        negq    %rax
        movq    %rbp, %rsp
        popq    %rbp
        ret

    1:
        pushq    %rbx

        movq    28(%rbx), %r8     /* rsi -> r8  #5 arg */
        movq    20(%rbx), %rcx    /* rdi -> rcx #4 arg */
        movq    16(%rbx), %rdx    /* rdx -> rdx #3 arg */
        movq    12(%rbx), %rsi    /* rcx -> rsi #2 arg */
        movq    8(%rbx), %rdi    /* rbx -> rdi #1 arg */
        
        call    *(%rax)

        addq    $40, %rsp      /* remove the parameters from stack */
        
        popq    %rbx
        movq    %rax, (%rbx)    /* save the return value */
        
        movq    %rbp,  %rsp
        popq    %rbp
        
        ret