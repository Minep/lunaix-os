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
        printf("fail to open %d\n", errno);
        return 1;
    }

    struct file_stat stat;
    if (fstat(fd, &stat) < 0) {
        printf("fail to get stat %d\n", errno);
        return 1;
    }

    printf("File: %s ", argv[1]);

    char* ftype = "directory";
    int mode = stat.st_mode >> 16;
    if ((mode & F_DEV)) {
        ftype = "mappable (sequential) device";

        if (!((mode & F_SVDEV) ^ F_SVDEV)) {
            ftype = "volumetric device";
        }

    } else if ((mode & F_SYMLINK)) {
        if (readlinkat(fd, NULL, buf, 256) < 0) {
            printf("fail to readlink %d\n", errno);
        } else {
            printf("-> %s", buf);
        }
        ftype = "symbolic link";
    } else if (mode == F_FILE) {
        ftype = "regular file";
    }

    printf("\nType: %s\n", ftype);
    printf("Size: %d;  Blocks: %d;  FS Block: %d;  IO Blocks: %d\n",
           stat.st_size,
           stat.st_blocks,
           stat.st_blksize,
           stat.st_ioblksize);

    dev_t* dev;
    if (!(mode & F_DEV)) {
        dev = &stat.st_dev;
    } else {
        dev = &stat.st_rdev;
    }
    printf("Device: %xh:%xh@%d;  Inode: %d;  Links: %d\n", 
           dev->meta, dev->unique, dev->index,
           stat.st_ino, stat.st_nlink);

    printf("Access: 0%o;  Uid: %d;  Gid: %d\n",
           stat.st_mode & 0xffff,
           stat.st_uid,
           stat.st_gid);

    printf("Access: %lu\n", stat.st_atim);
    printf("Modify: %lu\n", stat.st_mtim);
    printf("Create: %lu\n", stat.st_ctim);

    close(fd);
    return 0;
}