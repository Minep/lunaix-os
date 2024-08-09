#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

int
main(int argc, const char* argv[])
{
    if (argc != 2) {
        printf("expect a file name\n");
        return 1;
    }

    int err, fd;
    char* path = argv[1];
    struct file_stat stat;

    fd = open(path, O_RDONLY);
                
    if (fd < 0) {
        printf("open failed: %s (error: %d)", path, fd);
        return 1;
    }

    if (fstat(fd, &stat) < 0) {
        printf("fail to get stat %d\n", errno);
        return 1;
    }

    close(fd);

    if ((stat.mode & F_DIR)) {
        err = rmdir(path);
    }
    else {
        err = unlink(path);
    }

    if (err) {
        printf("fail to delete: %s (%d)", path, errno);
    }

    return err;
}