#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <lunaix/status.h>

static char alphabets[] = "abcdefghijklmnopqrstuvwxyz"
                          "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                          "01234567890";

#define NR_BUFSIZE   4096
#define NR_NAME_LEN  8
#define NR_REPEAT    5

int main()
{
    unsigned int buf[NR_BUFSIZE];
    char name[NR_NAME_LEN + 1];
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

    int nr_total = NR_REPEAT * NR_BUFSIZE / NR_NAME_LEN;

    int cnt = 0;
    for (int i = 0; i < NR_REPEAT; i++)
    {
        int n = read(fd, buf, 4096 * sizeof(int));
        int j = 0, k = 0;
        while (j < 4096) {
            name[k++] = alphabets[buf[j++] % 63];

            if (k < NR_NAME_LEN) {
                continue;
            }

            k = 0;
            cnt++;
            name[NR_NAME_LEN] = 0;

            printf("[%04d/%04d] creating: %s\r", cnt, nr_total, name);
            int fd2 = open(name, O_RDONLY | O_CREAT);
            
            if (fd2 < 0) 
            {
                printf("\n");
                if (errno == EDQUOT) {
                    printf("Out of quota\n");
                    return 0;
                }

                printf("Unable to open %d\n", errno);
                continue;
            }

            close(fd2);
        }
    }
    printf("\n");
    return 0;
}