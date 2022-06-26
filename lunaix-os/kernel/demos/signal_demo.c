#include <lunaix/lunistd.h>
#include <lunaix/proc.h>
#include <lunaix/signal.h>
#include <lunaix/spike.h>
#include <lunaix/syslog.h>
#include <lunaix/types.h>

LOG_MODULE("SIGDEMO")

void __USER__
sigchild_handler(int signum)
{
    kprintf(KINFO "SIGCHLD received\n");
}

void __USER__
sigsegv_handler(int signum)
{
    pid_t pid = getpid();
    kprintf(KWARN "SIGSEGV received on process %d\n", pid);
    _exit(signum);
}

void __USER__
sigalrm_handler(int signum)
{
    pid_t pid = getpid();
    kprintf(KWARN "I, pid %d, have received an alarm!\n", pid);
}

// FIXME: Race condition with signal (though rare!)
// For some reason, there is a chance that iret in soft_iret path
//   get unhappy when return from signal handler. Investigation is needed!

void __USER__
_signal_demo_main()
{
    signal(_SIGCHLD, sigchild_handler);
    signal(_SIGSEGV, sigsegv_handler);
    signal(_SIGALRM, sigalrm_handler);

    alarm(5);

    int status;
    pid_t p = 0;

    kprintf(KINFO "Child sleep 3s, parent pause.\n");
    if (!fork()) {
        sleep(3);
        _exit(0);
    }

    pause();

    kprintf("Parent resumed on SIGCHILD\n");

    for (int i = 0; i < 5; i++) {
        pid_t pid = 0;
        if (!(pid = fork())) {
            signal(_SIGSEGV, sigsegv_handler);
            sleep(i);
            if (i == 3) {
                i = *(int*)0xdeadc0de; // seg fault!
            }
            kprintf(KINFO "%d\n", i);
            _exit(0);
        }
        kprintf(KINFO "Forked %d\n", pid);
    }

    while ((p = wait(&status)) >= 0) {
        short code = WEXITSTATUS(status);
        if (WIFSIGNALED(status)) {
            kprintf(KINFO "Process %d terminated by signal, exit_code: %d\n",
                    p,
                    code);
        } else if (WIFEXITED(status)) {
            kprintf(KINFO "Process %d exited with code %d\n", p, code);
        } else {
            kprintf(KWARN "Process %d aborted with code %d\n", p, code);
        }
    }

    kprintf("done\n");

    _exit(0);
}