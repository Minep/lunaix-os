#include <lunaix/fctrl.h>
#include <lunaix/foptions.h>
#include <lunaix/lunistd.h>
#include <lunaix/proc.h>
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
            printf("Error: Fail to open (%d)\n", errno);
            break;
    }
}

void
sh_main()
{
    char buf[512];
    char *cmd, *argpart;

    printf("\n Simple shell. Use <PG_UP> or <PG_DOWN> to scroll.\n\n");

    while (1) {
        getcwd(pwd, 512);
        printf("%s$ ", pwd);
        size_t sz = read(stdin, buf, 512);
        if (sz < 0) {
            printf("fail to read user input (%d)\n", geterrno());
            return;
        }
        buf[sz - 1] = '\0';
        parse_cmdline(buf, &cmd, &argpart);
        if (cmd[0] == 0) {
            goto cont;
        }
        if (streq(cmd, "cd")) {
            if (chdir(argpart) < 0) {
                sh_printerr();
            }
        } else if (streq(cmd, "ls")) {
            int fd = open(argpart, 0);
            if (fd < 0) {
                sh_printerr();
            } else {
                struct dirent ent = { .d_offset = 0 };
                int status;
                while ((status = readdir(fd, &ent)) == 1) {
                    printf(" %s\n", ent.d_name);
                }

                if (status < 0)
                    sh_printerr();

                close(fd);
            }
        } else if (streq(cmd, "cat")) {
            int fd = open(argpart, 0);
            if (fd < 0) {
                sh_printerr();
            } else {
                int sz;
                while ((sz = read(fd, cat_buf, 1024)) == 1024) {
                    write(stdout, cat_buf, 1024);
                }
                if (sz < 0) {
                    sh_printerr();
                } else {
                    write(stdout, cat_buf, sz);
                }
                close(fd);
            }
        } else {
            printf("unknow command");
        }
    cont:
        printf("\n");
    }
}