#include <dirent.h>
#include <errno.h>
#include <stdio.h>

int
main(int argc, const char* argv[])
{
    const char* path = ".";
    if (argc > 1) {
        path = argv[1];
    }

    DIR* dir = opendir(path);

    if (!dir) {
        return errno;
    }

    struct dirent* dent;

    while ((dent = readdir(dir))) {
        if (dent->d_type == DT_DIR) {
            printf(" %s/\n", dent->d_name);
        } else if (dent->d_type == DT_SYMLINK) {
            printf(" %s@\n", dent->d_name);
        } else {
            printf(" %s\n", dent->d_name);
        }
    }

    int err = errno;
    if (err) {
        printf("failed: %d\n",err);
    }

    closedir(dir);

    return err;
}