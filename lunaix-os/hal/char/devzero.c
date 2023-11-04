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
    struct device* devzero = device_allocseq(NULL, NULL);
    devzero->ops.read_page = __zero_rd_pg;
    devzero->ops.read = __zero_rd;

    device_register(devzero, &def->class, "zero");

    return 0;
}

static struct device_def devzero_def = {
    .name = "zero",
    .class = DEVCLASSV(DEVIF_NON, DEVFN_PSEUDO, DEV_ZERO, DEV_BUILTIN_ZERO),
    .init = pdev_zerodev_init
};
EXPORT_DEVICE(zerodev, &devzero_def, load_onboot);
