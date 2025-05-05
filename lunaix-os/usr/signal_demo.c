#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

void
sigchild_handler(int signum)
{
    printf("SIGCHLD received\n");
}

void
sigsegv_handler(int signum)
{
    pid_t pid = getpid();
    printf("SIGSEGV received on process %d\n", pid);
    _exit(signum);
}

void
sigalrm_handler(int signum)
{
    pid_t pid = getpid();
    printf("I, pid %d, have received an alarm!\n", pid);
}

int
main()
{
    signal(SIGCHLD, sigchild_handler);
    signal(SIGSEGV, sigsegv_handler);
    signal(SIGALRM, sigalrm_handler);

    int status;
    pid_t p = 0;

    alarm(5);

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
            signal(SIGSEGV, sigsegv_handler);
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

    return 0;
}