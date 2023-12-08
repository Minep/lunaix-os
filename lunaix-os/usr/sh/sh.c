#include <errno.h>
#include <lunaix/ioctl.h>
#include <lunaix/lunaix.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

char pwd[512];
char cat_buf[1024];

/*
    Simple shell - (actually this is not even a shell)
    It just to make the testing more easy.
*/

#define WS_CHAR(c)                                                             \
    (c == ' ' || c == '\t' || c == '\n' || c == '\f' || c == '\v' || c == '\r')

void
strrtrim(char* str)
{
    size_t l = strlen(str);
    while (l < (size_t)-1) {
        char c = str[l];
        if (!c || WS_CHAR(c)) {
            l--;
            continue;
        }
        break;
    }
    str[l + 1] = '\0';
}

char*
strltrim_safe(char* str)
{
    size_t l = 0;
    char c = 0;
    while ((c = str[l]) && WS_CHAR(c)) {
        l++;
    }

    if (!l)
        return str;
    return strcpy(str, str + l);
}

void
parse_cmdline(char* line, char** cmd, char** arg_part)
{
    strrtrim(line);
    line = strltrim_safe(line);
    int l = 0;
    char c = 0;
    while ((c = line[l])) {
        if (c == ' ') {
            line[l++] = 0;
            break;
        }
        l++;
    }
    *cmd = line;
    if (c) {
        *arg_part = strltrim_safe(line + l);
    } else {
        *arg_part = NULL;
    }
}

void
sh_printerr()
{
    switch (errno) {
        case 0:
            break;
        case ENOTDIR:
            printf("Error: Not a directory\n");
            break;
        case ENOENT:
            printf("Error: No such file or directory\n");
            break;
        case EINVAL:
            printf("Error: Invalid parameter or operation\n");
            break;
        case ENOTSUP:
            printf("Error: Not supported\n");
            break;
        case EROFS:
            printf("Error: File system is read only\n");
            break;
        case ENOMEM:
            printf("Error: Out of memory\n");
            break;
        case EISDIR:
            printf("Error: This is a directory\n");
            break;
        default:
            printf("Error: (%d)\n", errno);
            break;
    }
}

void
sigint_handle(int signum)
{
    return;
}

void
sh_exec(const char* name, const char** argv)
{
    if (!strcmp(name, "cd")) {
        chdir(argv[0] ? argv[0] : ".");
        sh_printerr();
        return;
    }

    pid_t p;
    if (!(p = fork())) {
        if (execve(name, argv, NULL)) {
            sh_printerr();
        }
        _exit(1);
    }
    setpgid(p, getpgid());
    waitpid(p, NULL, 0);
}

void
sh_loop()
{
    char buf[512];
    char *cmd, *argpart;
    signal(SIGINT, sigint_handle);

    // set our shell as foreground process
    // (unistd.h:tcsetpgrp is essentially a wrapper of this)
    // stdout (by default, unless user did smth) is the tty we are currently at
    ioctl(stdout, TIOCSPGRP, getpgid());

    char* argv[] = {0, 0};

    while (1) {
        getcwd(pwd, 512);
        printf("[\033[2m%s\033[39;49m]$ ", pwd);
        int sz = read(stdin, buf, 511);

        if (sz < 0) {
            printf("fail to read user input (%d)\n", geterrno());
            return;
        }

        buf[sz] = '\0';

        // currently, this shell only support single argument
        parse_cmdline(buf, &cmd, &argv[0]);

        if (cmd[0] == 0) {
            printf("\n");
            goto cont;
        }

        // cmd=="exit"
        if (*(unsigned int*)cmd == 0x74697865U) {
            break;
        }

        sh_exec(cmd, (const char**)&argv);
    cont:
        printf("\n");
    }
}

void
main()
{
    sh_loop();
    _exit(0);
}