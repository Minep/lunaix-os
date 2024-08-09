#include <errno.h>
#include <unistd.h>
#include <stdio.h>

int
main(int argc, const char* argv[])
{
    if (argc != 2) {
        printf("expect a directory name\n");
        return 1;
    }

    int err;

    err = mkdir(argv[1]);
    if (err) {
        printf("unable to mkdir: %d\n", errno);
    }

    return err;
}