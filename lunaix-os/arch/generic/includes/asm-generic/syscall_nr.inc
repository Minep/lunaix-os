#include <lunaix/syscall.h>

#ifdef CONFIG_ARCH_BITS_64
#   define PTR     .8byte
#   define SIZE    8
#else
#   define PTR     .4byte
#   define SIZE    4
#endif

.section .data
    syscall_table:
        1:
        PTR 0
        PTR __lxsys_fork          /* 1 */
        PTR __lxsys_yield
        PTR __lxsys_sbrk
        PTR __lxsys_brk
        PTR __lxsys_getpid        /* 5 */
        PTR __lxsys_getppid
        PTR __lxsys_sleep
        PTR __lxsys_exit
        PTR __lxsys_wait          
        PTR __lxsys_waitpid       /* 10 */
        PTR __lxsys_sigreturn
        PTR __lxsys_sigprocmask
        PTR __lxsys_sys_sigaction
        PTR __lxsys_pause
        PTR __lxsys_kill          /* 15 */
        PTR __lxsys_alarm
        PTR __lxsys_sigpending
        PTR __lxsys_sigsuspend
        PTR __lxsys_open
        PTR __lxsys_close         /* 20 */
        PTR __lxsys_read
        PTR __lxsys_write
        PTR __lxsys_sys_readdir
        PTR __lxsys_mkdir
        PTR __lxsys_lseek         /* 25 */
        PTR __lxsys_geterrno
        PTR __lxsys_readlink
        PTR __lxsys_readlinkat
        PTR __lxsys_rmdir
        PTR __lxsys_unlink        /* 30 */
        PTR __lxsys_unlinkat
        PTR __lxsys_link
        PTR __lxsys_fsync
        PTR __lxsys_dup
        PTR __lxsys_dup2          /* 35 */
        PTR __lxsys_realpathat
        PTR __lxsys_symlink
        PTR __lxsys_chdir
        PTR __lxsys_fchdir
        PTR __lxsys_getcwd        /* 40 */
        PTR __lxsys_rename
        PTR __lxsys_mount
        PTR __lxsys_unmount
        PTR __lxsys_getxattr
        PTR __lxsys_setxattr      /* 45 */
        PTR __lxsys_fgetxattr
        PTR __lxsys_fsetxattr
        PTR __lxsys_ioctl
        PTR __lxsys_getpgid
        PTR __lxsys_setpgid       /* 50 */
        PTR __lxsys_syslog
        PTR __lxsys_sys_mmap
        PTR __lxsys_munmap
        PTR __lxsys_execve
        PTR __lxsys_fstat         /* 55 */
        PTR __lxsys_pollctl
        PTR __lxsys_th_create
        PTR __lxsys_th_self
        PTR __lxsys_th_exit
        PTR __lxsys_th_join       /* 60 */
        PTR __lxsys_th_kill
        PTR __lxsys_th_detach
        PTR __lxsys_th_sigmask
        2:
        .rept __SYSCALL_MAX - (2b - 1b) / SIZE
            .long 0
        .endr