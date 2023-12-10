#include <lunaix/fs/iso9660.h>
#include <lunaix/fs/probe_boot.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/syslog.h>

LOG_MODULE("PROBE")

#define LUNAIX_ID 0x414e554cUL // "LUNA"

struct device*
probe_boot_medium()
{
    struct device_meta* block_cat = device_getbyname(NULL, "block", 5);
    if (!block_cat) {
        return NULL;
    }

    struct iso_vol_primary* volp = valloc(ISO9660_BLKSZ);

    struct device* dev = NULL;
    struct device_meta *pos, *n;
    llist_for_each(pos, n, &block_cat->children, siblings)
    {
        dev = resolve_device(pos);
        if (!dev) {
            continue;
        }

        int errno =
          dev->ops.read(dev, (void*)volp, ISO9660_READ_OFF, ISO9660_BLKSZ);
        if (errno < 0) {
            kprintf(KINFO "failed %xh:%xh, /dev/%s",
                    dev->ident.fn_grp,
                    dev->ident.unique,
                    dev->name.value);
            dev = NULL;
            goto done;
        }

        if (*(u32_t*)volp->header.std_id != ISO_SIGNATURE_LO) {
            continue;
        }

        if (*(u32_t*)volp->sys_id == LUNAIX_ID) {
            kprintf(KINFO "%xh:%xh, /dev/%s, %s",
                    dev->ident.fn_grp,
                    dev->ident.unique,
                    dev->name.value,
                    (char*)volp->vol_id);
            break;
        }
    }

done:
    vfree(volp);
    return dev;
}