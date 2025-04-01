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
        goto done;
    }

    struct dirent* dent;
    int i = 0, sz;
    char bf[100];

    while ((dent = readdir(dir))) {
        if (dent->d_type == DT_DIR) {
            sz = snprintf(bf, 100, "%s/", dent->d_name);
        } else if (dent->d_type == DT_SYMLINK) {
            sz = snprintf(bf, 100, "%s@", dent->d_name);
        } else {
            sz = snprintf(bf, 100, "%s", dent->d_name);
        }

        bf[sz] = 0;
        printf("%-15s ", bf);

        i++;
        if ((i % 4) == 0) {
            printf("\n");
        }
    }

    if ((i % 4) != 0) {
        printf("\n");
    }

done:
    int err = errno;
    if (err) {
        printf("failed: %d\n",err);
    }

    if (dir)
        closedir(dir);

    return err;
}