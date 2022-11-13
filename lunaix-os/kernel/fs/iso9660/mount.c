#include <lunaix/block.h>
#include <lunaix/fs.h>
#include <lunaix/fs/iso9660.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/spike.h>

#include <lunaix/mm/cake.h>

struct cake_pile* drec_cache_pile;

extern void
iso9660_init_inode(struct v_superblock* vsb, struct v_inode* inode);

u32_t
iso9660_rd_capacity(struct v_superblock* vsb)
{
    struct iso_superblock* isovsb = (struct iso_superblock*)vsb->data;
    return isovsb->volume_size;
}

int
iso9660_mount(struct v_superblock* vsb, struct v_dnode* mount_point)
{
    struct device* dev = vsb->dev;
    struct iso_vol* vdesc = (struct iso_vol*)valloc(ISO9660_BLKSZ);
    struct iso_vol_primary* vprim = NULL;
    u32_t lba = 16;
    int errno = 0;
    do {
        errno = dev->read(dev, vdesc, ISO9660_BLKSZ * lba, ISO9660_BLKSZ);
        if (errno < 0) {
            errno = EIO;
            goto done;
        }
        if (*(u32_t*)vdesc->std_id != ISO_SIGNATURE_LO) {
            errno = ENODEV;
            goto done;
        }
        if (vdesc->type == ISO_VOLPRIM) {
            vprim = (struct iso_vol_primary*)vdesc;
            break;
        }
        lba++;
    } while (vdesc->type != ISO_VOLTERM);

    if (!vprim) {
        errno = EINVAL;
        goto done;
    }

    struct iso_superblock* isovsb = valloc(sizeof(*isovsb));
    isovsb->lb_size = vprim->lb_size.le;
    isovsb->volume_size = vprim->vol_size.le * isovsb->lb_size;

    vsb->data = isovsb;
    vsb->ops.init_inode = iso9660_init_inode;
    vsb->ops.read_capacity = iso9660_rd_capacity;

    struct v_inode* rootino = vfs_i_alloc(vsb);
    struct iso_var_mdu* mdu = (struct iso_var_mdu*)vprim->root_record;
    struct iso_drecord* dir = iso9660_get_drecord(mdu);

    if (!dir) {
        vfree(isovsb);
        errno = EINVAL;
        goto done;
    }

    struct iso_drecache drecache;
    iso9660_fill_drecache(&drecache, dir, mdu->len);

    if ((errno = iso9660_fill_inode(rootino, &drecache, 0)) < 0) {
        vfree(isovsb);
        errno = EINVAL;
        goto done;
    }

    if ((errno = iso9660_setup_dnode(mount_point, rootino)) < 0) {
        vfree(isovsb);
        errno = EINVAL;
        goto done;
    }

    vfs_i_addhash(rootino);
    return 0;

done:
    vfree(vdesc);
    return errno;
}

int
iso9660_unmount(struct v_superblock* vsb)
{
    vfree(vsb->data);
}

void
iso9660_init()
{
    drec_cache_pile =
      cake_new_pile("iso_drec", sizeof(struct iso_drecache), 1, 0);

    struct filesystem* fs = fsm_new_fs("iso9660", -1);
    fs->types |= FSTYPE_ROFS;
    fs->mount = iso9660_mount;
    fs->unmount = iso9660_unmount;

    fsm_register(fs);
}