#include <lunaix/fs.h>
#include <lunaix/fs/ramfs.h>
#include <lunaix/fs/twifs.h>

void
fsm_register_all()
{
    ramfs_init();
    twifs_init();
    // Add more fs implementation
}