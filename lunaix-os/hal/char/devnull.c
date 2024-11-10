#include <lunaix/device.h>
#include <lunaix/mm/pagetable.h>

static int
__null_wr_pg(struct device* dev, void* buf, size_t offset)
{
    // do nothing
    return PAGE_SIZE;
}

static int
__null_wr(struct device* dev, void* buf, size_t offset, size_t len)
{
    // do nothing
    return len;
}

static int
__null_rd_pg(struct device* dev, void* buf, size_t offset)
{
    // do nothing
    return 0;
}

static int
__null_rd(struct device* dev, void* buf, size_t offset, size_t len)
{
    // do nothing
    return 0;
}

static int
pdev_nulldev_create(struct device_def* def, morph_t* obj)
{
    struct device* devnull = device_allocseq(NULL, NULL);
    devnull->ops.write_page = __null_wr_pg;
    devnull->ops.write = __null_wr;
    devnull->ops.read_page = __null_rd_pg;
    devnull->ops.read = __null_rd;

    register_device(devnull, &def->class, "null");

    return 0;
}

static struct device_def devnull_def = {
    def_device_name("edendi"),
    def_device_class(LUNAIX, PSEUDO, NIHIL),
    def_on_create(pdev_nulldev_create)
};
EXPORT_DEVICE(nulldev, &devnull_def, load_onboot);
