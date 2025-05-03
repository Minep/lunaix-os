#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <lunaix/status.h>

static char alphabets[] = "abcdefghijklmnopqrstuvwxyz"
                          "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                          "01234567890";

int main()
{
    unsigned int buf[4096];
    char name[8];
    int fd = open("/dev/rand", O_RDONLY);

    if (mkdir("testdir") && errno != EEXIST)
    {
        printf("Unable to mkdir %d\n", errno);
        _exit(1);
    }

    if (chdir("testdir"))
    {
        printf("Unable to chdir %d\n", errno);
        _exit(1);
    }

    int cnt = 0;
    for (int i = 0; i < 1; i++)
    {
        int n = read(fd, buf, 4096 * sizeof(int));
        int j = 0, k = 0;
        while (j < 4096) {
            name[k++] = alphabets[buf[j++] % 63];

            if (k < 7) {
                continue;
            }

            k = 0;
            cnt++;
            name[7] = 0;

            printf("[%03d] creating: %s\n", cnt, name);
            int fd2 = open(name, O_RDONLY | O_CREAT);
            if (fd2 < 0) {
                printf("Unable to open %d\n", errno);
                continue;
            }

            close(fd2);
        }
    }
    
    return 0;
}