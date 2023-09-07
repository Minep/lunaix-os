#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static char buf[256];

int
main(int argc, char* argv[])
{
    if (argc <= 1) {
        printf("missing operand\n");
        return 1;
    }

    int fd = open(argv[1], FO_RDONLY | FO_NOFOLLOW);
    if (fd < 0) {
        printf("fail to open %d\n", fd);
        return 1;
    }

    struct file_stat stat;
    if (fstat(fd, &stat) < 0) {
        printf("fail to get stat %d\n", errno);
        return 1;
    }

    printf("File: %s ", argv[1]);

    char* ftype = "directory";
    int mode = stat.mode;
    if ((mode & F_MDEV)) {
        if (!((mode & F_SEQDEV) ^ F_SEQDEV)) {
            ftype = "sequential device";
        } else if (!((mode & F_VOLDEV) ^ F_VOLDEV)) {
            ftype = "volumetric device";
        } else {
            ftype = "regular device";
        }
    } else if ((mode & F_MSLNK)) {
        if (readlinkat(fd, NULL, buf, 256) < 0) {
            printf("fail to readlink %d\n", errno);
        } else {
            printf("-> %s", buf);
        }
        ftype = "symbolic link";
    } else if ((mode & F_MFILE)) {
        ftype = "regular file";
    }

    printf("\nType: %s\n", ftype);
    printf("Size: %d;  Blocks: %d;  FS Block: %d;  IO Blocks: %d\n",
           stat.st_size,
           stat.st_blocks,
           stat.st_blksize,
           stat.st_ioblksize);
    printf("Inode: %d;  ", stat.st_ino);

    dev_t* dev;
    if (!(stat.mode & F_MDEV)) {
        dev = &stat.st_dev;
    } else {
        dev = &stat.st_rdev;
    }

    printf("Device: %xh:%d:%d;\n",
           dev->meta,
           dev->devident >> 16,
           dev->devident & 0xffff);

    close(fd);
    return 0;
}