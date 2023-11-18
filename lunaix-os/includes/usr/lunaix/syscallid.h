#ifndef __LUNAIX_SYSCALLID_H
#define __LUNAIX_SYSCALLID_H

#define __SYSCALL_fork 1
#define __SYSCALL_yield 2
#define __SYSCALL_sbrk 3
#define __SYSCALL_brk 4
#define __SYSCALL_getpid 5
#define __SYSCALL_getppid 6
#define __SYSCALL_sleep 7
#define __SYSCALL__exit 8
#define __SYSCALL_wait 9
#define __SYSCALL_waitpid 10

#define __SYSCALL_sigreturn 11
#define __SYSCALL_sigprocmask 12
#define __SYSCALL_sys_sigaction 13
#define __SYSCALL_pause 14
#define __SYSCALL_kill 15
#define __SYSCALL_alarm 16
#define __SYSCALL_sigpending 17
#define __SYSCALL_sigsuspend 18
#define __SYSCALL_open 19
#define __SYSCALL_close 20

#define __SYSCALL_read 21
#define __SYSCALL_write 22
#define __SYSCALL_sys_readdir 23
#define __SYSCALL_mkdir 24
#define __SYSCALL_lseek 25
#define __SYSCALL_geterrno 26
#define __SYSCALL_readlink 27
#define __SYSCALL_readlinkat 28
#define __SYSCALL_rmdir 29

#define __SYSCALL_unlink 30
#define __SYSCALL_unlinkat 31
#define __SYSCALL_link 32
#define __SYSCALL_fsync 33
#define __SYSCALL_dup 34
#define __SYSCALL_dup2 35
#define __SYSCALL_realpathat 36
#define __SYSCALL_symlink 37
#define __SYSCALL_chdir 38
#define __SYSCALL_fchdir 39
#define __SYSCALL_getcwd 40
#define __SYSCALL_rename 41
#define __SYSCALL_mount 42
#define __SYSCALL_unmount 43
#define __SYSCALL_getxattr 44
#define __SYSCALL_setxattr 45
#define __SYSCALL_fgetxattr 46
#define __SYSCALL_fsetxattr 47

#define __SYSCALL_ioctl 48
#define __SYSCALL_getpgid 49
#define __SYSCALL_setpgid 50

#define __SYSCALL_syslog 51

#define __SYSCALL_sys_mmap 52
#define __SYSCALL_munmap 53

#define __SYSCALL_execve 54

#define __SYSCALL_fstat 55
#define __SYSCALL_pollctl 56

#define __SYSCALL_MAX 0x100

#endif /* __LUNAIX_SYSCALLID_H */
