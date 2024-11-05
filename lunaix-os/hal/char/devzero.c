#include <lunaix/device.h>
#include <lunaix/mm/pagetable.h>

#include <klibc/string.h>

static int
__zero_rd_pg(struct device* dev, void* buf, size_t offset)
{
    memset(buf, 0, PAGE_SIZE);
    return PAGE_SIZE;
}

static int
__zero_rd(struct device* dev, void* buf, size_t offset, size_t len)
{
    memset(buf, 0, len);
    return len;
}

static int
pdev_zerodev_create(struct device_def* def, morph_t* obj)
{
    struct device* devzero = device_allocseq(NULL, NULL);
    devzero->ops.read_page = __zero_rd_pg;
    devzero->ops.read = __zero_rd;

    register_device(devzero, &def->class, "zero");

    return 0;
}

static struct device_def devzero_def = {
    def_device_name("nihil"),
    def_device_class(NON, PSEUDO, ZERO),
    def_on_create(pdev_zerodev_create)
};
EXPORT_DEVICE(zerodev, &devzero_def, load_onboot);
