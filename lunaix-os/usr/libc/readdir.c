#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/lxdirent.h>

DIR*
opendir(const char* dir)
{
    static DIR _dir;
    int fd = open(dir, O_RDONLY);
    if (fd < 0) {
        return NULL;
    }

    _dir = (DIR){ .dirfd = fd, .prev_res = 0 };
    return &_dir;
}

struct dirent*
readdir(DIR* dir)
{
    static struct dirent _dirent;
    if (!dir) {
        return NULL;
    }

    struct lx_dirent _lxd;
    int more = sys_readdir(dir->dirfd, &_lxd);

    _dirent.d_type = _lxd.d_type;
    strncpy(_dirent.d_name, _lxd.d_name, 256);

    if (more || dir->prev_res) {
        dir->prev_res = more;
        return &_dirent;
    }

    if (!dir->prev_res) {
        return NULL;
    }
}