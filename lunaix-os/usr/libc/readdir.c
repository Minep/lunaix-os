#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/lxdirent.h>
#include <unistd.h>

DIR*
opendir(const char* dir)
{
    static DIR _dir;
    int fd = open(dir, O_RDONLY);
    if (fd < 0) {
        return NULL;
    }

    _dir = (DIR){ .dirfd = fd };
    return &_dir;
}

int
closedir(DIR* dirp)
{
    if (!dirp || dirp->dirfd == -1) {
        // TODO migrate the status.h
        return -1;
    }

    int err = close(dirp->dirfd);

    if (!err) {
        dirp->dirfd = -1;
        return 0;
    }

    return -1;
}

struct dirent*
readdir(DIR* dir)
{
    static struct dirent _dirent;
    if (!dir) {
        return NULL;
    }

    struct lx_dirent* _lxd = &dir->_lxd;

    int more = sys_readdir(dir->dirfd, _lxd);

    _dirent.d_type = _lxd->d_type;
    strncpy(_dirent.d_name, _lxd->d_name, 256);

    if (more) {
        return &_dirent;
    }

    return NULL;
}