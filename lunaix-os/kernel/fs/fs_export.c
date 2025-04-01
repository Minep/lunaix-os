#include <lunaix/foptions.h>
#include <lunaix/fs.h>
#include <lunaix/fs/twifs.h>

extern struct llist_header all_mnts;

static void
__twimap_read_mounts(struct twimap* map)
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

static int
__twimap_gonext_mounts(struct twimap* map)
{
    struct v_mount* mnt = twimap_index(map, struct v_mount*);
    if (mnt->list.next == &all_mnts) {
        return 0;
    }
    map->index = container_of(mnt->list.next, struct v_mount, list);
    return 1;
}

static void
__twimap_reset_mounts(struct twimap* map)
{
    map->index = container_of(all_mnts.next, struct v_mount, list);
}

void
__twimap_read_version(struct twimap* map)
{
    twimap_printf(map,
                  "Lunaix "
                  CONFIG_LUNAIX_VER
                  " (gnu-cc " __VERSION__ ") " __DATE__ " " __TIME__);
}

void
vfs_export_attributes()
{
    twimap_export_list (NULL, mounts,  FSACL_ugR, NULL);
    twimap_export_value(NULL, version, FSACL_ugR, NULL);
}
EXPORT_TWIFS_PLUGIN(vfs_general, vfs_export_attributes);