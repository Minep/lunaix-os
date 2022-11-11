#include <lunaix/lunaix.h>
#include <lunaix/lunistd.h>
#include <lunaix/signal.h>
#include <lunaix/spike.h>
#include <lunaix/types.h>
#include <ulibc/stdio.h>

void __USER__
sigchild_handler(int signum)
{
    printf("SIGCHLD received\n");
}

void __USER__
sigsegv_handler(int signum)
{
    pid_t pid = getpid();
    printf("SIGSEGV received on process %d\n", pid);
    _exit(signum);
}

void __USER__
sigalrm_handler(int signum)
{
    pid_t pid = getpid();
    printf("I, pid %d, have received an alarm!\n", pid);
}

void __USER__
_signal_demo_main()
{
    signal(_SIGCHLD, sigchild_handler);
    signal(_SIGSEGV, sigsegv_handler);
    signal(_SIGALRM, sigalrm_handler);

    alarm(5);

    int status;
    pid_t p = 0;

    printf("Child sleep 3s, parent pause.\n");
    if (!fork()) {
        sleep(3);
        _exit(0);
    }

    pause();

    printf("Parent resumed on SIGCHILD\n");

    for (int i = 0; i < 5; i++) {
        pid_t pid = 0;
        if (!(pid = fork())) {
            signal(_SIGSEGV, sigsegv_handler);
            sleep(i);
            if (i == 3) {
                i = *(int*)0xdeadc0de; // seg fault!
            }
            printf("%d\n", i);
            _exit(0);
        }
        printf("Forked %d\n", pid);
    }

    while ((p = wait(&status)) >= 0) {
        short code = WEXITSTATUS(status);
        if (WIFSIGNALED(status)) {
            printf("Process %d terminated by signal, exit_code: %d\n", p, code);
        } else if (WIFEXITED(status)) {
            printf("Process %d exited with code %d\n", p, code);
        } else {
            printf("Process %d aborted with code %d\n", p, code);
        }
    }

    printf("done\n");
}