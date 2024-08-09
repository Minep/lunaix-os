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

int
parse_cmdline(char* line, char** args)
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

    args[0] = line;
    if (c && l) {
        args[1] = strltrim_safe(line + l);
    } else {
        args[1] = NULL;
    }

    return !!l;
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
sh_exec(const char** argv)
{
    static int prev_exit;
    const char* envp[] = { 0 };
    char* name = argv[0];
    if (!strcmp(name, "cd")) {
        chdir(argv[1] ? argv[1] : ".");
        sh_printerr();
        return;
    }

    if (!strcmp(name, "?")) {
        printf("%d\n", prev_exit);
        return;
    }

    char buffer[1024];
    strcpy(buffer, "/usr/bin/");
    strcpy(&buffer[9], name);

    pid_t p;
    int res;
    if (!(p = fork())) {
        if (execve(buffer, argv, envp)) {
            sh_printerr();
        }
        _exit(1);
    }
    setpgid(p, getpgid());
    waitpid(p, &res, 0);

    prev_exit = WEXITSTATUS(res);
}

static char*
sanity_filter(char* buf)
{
    int off = 0, i = 0;
    char c;
    do {
        c = buf[i];
        
        if ((32 <= c && c <= 126) || !c) {
            buf[i - off] = c;
        }
        else {
            off++;
        }

        i++;
    } while(c);

    return buf;
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

    char* argv[] = {0, 0, 0};

    while (1) {
        getcwd(pwd, 512);
        printf("[%s]$ ", pwd);
        int sz = read(stdin, buf, 511);

        if (sz < 0) {
            printf("fail to read user input (%d)\n", geterrno());
            return;
        }

        buf[sz] = '\0';
        sanity_filter(buf);

        // currently, this shell only support single argument
        if (!parse_cmdline(buf, argv)) {
            printf("\n");
            goto cont;
        }

        // cmd=="exit"
        if (*(unsigned int*)argv[0] == 0x74697865U) {
            break;
        }

        sh_exec((const char**)argv);
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