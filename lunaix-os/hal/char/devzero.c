#include <lunaix/device.h>
#include <lunaix/mm/page.h>

#include <klibc/string.h>

static int
__zero_rd_pg(struct device* dev, void* buf, size_t offset)
{
    memset(&((u8_t*)buf)[offset], 0, PG_SIZE);
    return PG_SIZE;
}

static int
__zero_rd(struct device* dev, void* buf, size_t offset, size_t len)
{
    memset(&((u8_t*)buf)[offset], 0, len);
    return len;
}

static int
pdev_zerodev_init(struct device_def* def)
{
    struct device* devzero = device_addseq(NULL, &def->class, NULL, "zero");
    devzero->ops.read_page = __zero_rd_pg;
    devzero->ops.read = __zero_rd;

    return 0;
}

static struct device_def devzero_def = {
    .name = "zero",
    .class = DEVCLASS(DEVIF_NON, DEVFN_PSEUDO, DEV_BUILTIN, 1),
    .init = pdev_zerodev_init
};
EXPORT_DEVICE(zerodev, &devzero_def, load_earlystage);
