#define __ASM__
#include <lunaix/syscall.h>

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
        .long __lxsys_signal
        .long __lxsys_pause
        .long __lxsys_kill          /* 15 */
        .long __lxsys_alarm
        .long __lxsys_sigpending
        .long __lxsys_sigsuspend
        .long __lxsys_open
        .long __lxsys_close         /* 20 */
        .long __lxsys_read
        .long __lxsys_write
        .long __lxsys_readdir
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
        2:
        .rept __SYSCALL_MAX - (2b - 1b)/4
            .long 0
        .endr

.global syscall_hndlr

.section .text
    syscall_hndlr:
        pushl %ebp
        movl 8(%esp), %ebp

        movl  (%ebp), %eax          /* eax: call code as well as the return value from syscall */
        cmpl  $__SYSCALL_MAX, %eax
        jae 2f

        shll $2, %eax
        addl $syscall_table, %eax
        cmpl $0, (%eax)
        jne 1f
    2:    
        neg   %eax
        popl  %ebp
        ret
    1:
        pushl 24(%ebp)      /* esi - #6 arg */
        pushl 20(%ebp)      /* ebp - #5 arg */
        pushl 16(%ebp)      /* edi - #4 arg */
        pushl 12(%ebp)      /* edx - #3 arg */
        pushl 8(%ebp)       /* ecx - #2 arg */
        pushl 4(%ebp)       /* ebx - #1 arg */
        
        call (%eax)

        movl %eax, (%ebp)    /* save the return value */

        addl $24, %esp      /* remove the parameters from stack */

        popl %ebp
        
        ret