#ifndef SYSCALL
#   error "SYSCALL missing pointer type"
#endif

SYSCALL 0
SYSCALL __lxsys_fork          /* 1 */
SYSCALL __lxsys_yield
SYSCALL __lxsys_sbrk
SYSCALL __lxsys_brk
SYSCALL __lxsys_getpid        /* 5 */
SYSCALL __lxsys_getppid
SYSCALL __lxsys_sleep
SYSCALL __lxsys_exit
SYSCALL __lxsys_wait          
SYSCALL __lxsys_waitpid       /* 10 */
SYSCALL __lxsys_sigreturn
SYSCALL __lxsys_sigprocmask
SYSCALL __lxsys_sys_sigaction
SYSCALL __lxsys_pause
SYSCALL __lxsys_kill          /* 15 */
SYSCALL __lxsys_alarm
SYSCALL __lxsys_sigpending
SYSCALL __lxsys_sigsuspend
SYSCALL __lxsys_open
SYSCALL __lxsys_close         /* 20 */
SYSCALL __lxsys_read
SYSCALL __lxsys_write
SYSCALL __lxsys_sys_readdir
SYSCALL __lxsys_mkdir
SYSCALL __lxsys_lseek         /* 25 */
SYSCALL __lxsys_geterrno
SYSCALL __lxsys_readlink
SYSCALL __lxsys_readlinkat
SYSCALL __lxsys_rmdir
SYSCALL __lxsys_unlink        /* 30 */
SYSCALL __lxsys_unlinkat
SYSCALL __lxsys_link
SYSCALL __lxsys_fsync
SYSCALL __lxsys_dup
SYSCALL __lxsys_dup2          /* 35 */
SYSCALL __lxsys_realpathat
SYSCALL __lxsys_symlink
SYSCALL __lxsys_chdir
SYSCALL __lxsys_fchdir
SYSCALL __lxsys_getcwd        /* 40 */
SYSCALL __lxsys_rename
SYSCALL __lxsys_mount
SYSCALL __lxsys_unmount
SYSCALL __lxsys_getxattr
SYSCALL __lxsys_setxattr      /* 45 */
SYSCALL __lxsys_fgetxattr
SYSCALL __lxsys_fsetxattr
SYSCALL __lxsys_ioctl
SYSCALL __lxsys_getpgid
SYSCALL __lxsys_setpgid       /* 50 */
SYSCALL __lxsys_syslog
SYSCALL __lxsys_sys_mmap
SYSCALL __lxsys_munmap
SYSCALL __lxsys_execve
SYSCALL __lxsys_fstat         /* 55 */
SYSCALL __lxsys_pollctl
SYSCALL __lxsys_th_create
SYSCALL __lxsys_th_self
SYSCALL __lxsys_th_exit
SYSCALL __lxsys_th_join       /* 60 */
SYSCALL __lxsys_th_kill
SYSCALL __lxsys_th_detach
SYSCALL __lxsys_th_sigmask
SYSCALL __lxsys_getuid
SYSCALL __lxsys_getgid
SYSCALL __lxsys_geteuid
SYSCALL __lxsys_getegid
SYSCALL __lxsys_getgroups
SYSCALL __lxsys_setuid
SYSCALL __lxsys_setgid
SYSCALL __lxsys_seteuid
SYSCALL __lxsys_setegid
SYSCALL __lxsys_setgroups