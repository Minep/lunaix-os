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
            printf(" \033[3m%s\033[39;49m\n", dent->d_name);
        } else if (dent->d_type == DT_SYMLINK) {
            printf(" \033[13m%s@\033[39;49m\n", dent->d_name);
        } else {
            printf(" %s\n", dent->d_name);
        }
    }

    closedir(dir);

    return 0;
}