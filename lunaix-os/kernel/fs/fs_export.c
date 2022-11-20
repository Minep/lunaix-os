#include <lunaix/foptions.h>
#include <lunaix/fs.h>
#include <lunaix/fs/twifs.h>

extern struct llist_header all_mnts;

void
__mount_read(struct twimap* map)
{
    char path[512];
    struct v_mount* mnt = twimap_index(map, struct v_mount*);
    size_t len = vfs_get_path(mnt->mnt_point, path, 511, 0);
    path[len] = '\0';
    twimap_printf(map, "%s at %s", mnt->super_block->fs->fs_name.value, path);
    if ((mnt->flags & MNT_RO)) {
        twimap_printf(map, ", ro");
    } else {
        twimap_printf(map, ", rw");
    }
    twimap_printf(map, "\n");
}

int
__mount_next(struct twimap* map)
{
    struct v_mount* mnt = twimap_index(map, struct v_mount*);
    if (mnt->list.next == &all_mnts) {
        return 0;
    }
    map->index = container_of(mnt->list.next, struct v_mount, list);
    return 1;
}

void
__mount_reset(struct twimap* map)
{
    map->index = container_of(all_mnts.next, struct v_mount, list);
}

void
__version_rd(struct twimap* map)
{
    twimap_printf(map,
                  "LunaixOS version %s (%s-gnu-gcc %s) %s %s",
                  LUNAIX_VER,
                  PLATFORM_TARGET,
                  __VERSION__,
                  __DATE__,
                  __TIME__);
}

void
vfs_export_attributes()
{
    struct twimap* map = twifs_mapping(NULL, NULL, "mounts");
    map->read = __mount_read;
    map->go_next = __mount_next;
    map->reset = __mount_reset;

    map = twifs_mapping(NULL, NULL, "version");
    map->read = __version_rd;
}