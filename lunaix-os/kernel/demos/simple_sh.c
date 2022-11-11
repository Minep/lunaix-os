#include <lunaix/fctrl.h>
#include <lunaix/foptions.h>
#include <lunaix/ioctl.h>
#include <lunaix/lunaix.h>
#include <lunaix/lunistd.h>
#include <lunaix/signal.h>
#include <lunaix/status.h>

#include <klibc/string.h>
#include <ulibc/stdio.h>

char pwd[512];
char cat_buf[1024];

/*
    Simple shell - (actually this is not even a shell)
    It just to make the testing more easy.
*/

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
    *arg_part = strltrim_safe(line + l);
}

void
sh_printerr()
{
    int errno = geterrno();
    switch (errno) {
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
do_cat(const char* file)
{
    int fd = open(file, 0);
    if (fd < 0) {
        sh_printerr();
    } else {
        int sz;
        while ((sz = read(fd, cat_buf, 1024)) > 0) {
            write(stdout, cat_buf, sz);
        }
        if (sz < 0) {
            sh_printerr();
        }
        close(fd);
        printf("\n");
    }
}

void
do_ls(const char* path)
{
    int fd = open(path, 0);
    if (fd < 0) {
        sh_printerr();
    } else {
        struct dirent ent = { .d_offset = 0 };
        int status;
        while ((status = readdir(fd, &ent)) == 1) {
            if (ent.d_type == DT_DIR) {
                printf(" \033[3m%s\033[39;49m\n", ent.d_name);
            } else {
                printf(" %s\n", ent.d_name);
            }
        }

        if (status < 0)
            sh_printerr();

        close(fd);
    }
}

void
sh_loop()
{
    char buf[512];
    char *cmd, *argpart;
    pid_t p;
    signal(_SIGINT, sigint_handle);

    // set our shell as foreground process
    // (unistd.h:tcsetpgrp is essentially a wrapper of this)
    // stdout (by default, unless user did smth) is the tty we are currently at
    ioctl(stdout, TIOCSPGRP, getpgid());

    while (1) {
        getcwd(pwd, 512);
        printf("[\033[2m%s\033[39;49m]$ ", pwd);
        size_t sz = read(stdin, buf, 511);
        if (sz < 0) {
            printf("fail to read user input (%d)\n", geterrno());
            return;
        }
        buf[sz] = '\0';
        parse_cmdline(buf, &cmd, &argpart);
        if (cmd[0] == 0) {
            printf("\n");
            goto cont;
        }
        if (streq(cmd, "cd")) {
            if (chdir(argpart) < 0) {
                sh_printerr();
            }
            goto cont;
        } else if (streq(cmd, "clear")) {
            ioctl(stdout, TIOCCLSBUF);
            goto cont;
        } else if (streq(cmd, "ls")) {
            if (!(p = fork())) {
                do_ls(argpart);
                _exit(0);
            }
        } else if (streq(cmd, "cat")) {
            if (!(p = fork())) {
                do_cat(argpart);
                _exit(0);
            }
        } else {
            printf("unknow command\n");
            goto cont;
        }
        setpgid(p, getpgid());
        waitpid(p, NULL, 0);
    cont:
        printf("\n");
    }
}

void
sh_main()
{
    printf("\n Simple shell. Use <PG_UP> or <PG_DOWN> to scroll.\n\n");
    if (!fork()) {
        sh_loop();
        _exit(0);
    }
    wait(NULL);
}