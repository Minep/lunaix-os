#include <lunaix/fs/iso9660.h>
#include <lunaix/fs/probe_boot.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/syslog.h>

LOG_MODULE("PROBE")

#define LUNAIX_ID 0x414e554cUL // "LUNA"

struct device*
probe_boot_medium()
{
    struct device* block_cat = device_getbyname(NULL, "block", 5);
    if (!block_cat) {
        return NULL;
    }

    struct iso_vol_primary* volp = valloc(ISO9660_BLKSZ);

    struct device *pos, *n;
    llist_for_each(pos, n, &block_cat->children, siblings)
    {
        int errno =
          pos->read(pos, (void*)volp, ISO9660_READ_OFF, ISO9660_BLKSZ);
        if (errno < 0) {
            kprintf(KWARN "can not probe %x:%s (%d)\n",
                    pos->dev_id,
                    pos->name.value,
                    errno);
            pos = NULL;
            goto done;
        }

        if (*(u32_t*)volp->header.std_id != ISO_SIGNATURE_LO) {
            continue;
        }

        if (*(u32_t*)volp->sys_id == LUNAIX_ID) {
            kprintf(KINFO "[%x:%s] %s\n",
                    pos->dev_id,
                    pos->name.value,
                    (char*)volp->vol_id);
            break;
        }
    }

done:
    vfree(volp);
    return pos;
}