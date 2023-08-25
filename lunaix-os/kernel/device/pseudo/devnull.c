#include <lunaix/device.h>
#include <lunaix/mm/page.h>

static int
__null_wr_pg(struct device* dev, void* buf, size_t offset)
{
    // do nothing
    return PG_SIZE;
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

void
pdev_nulldev_init()
{
    struct device* devnull = device_addseq(NULL, NULL, "null");
    devnull->ops.write_page = __null_wr_pg;
    devnull->ops.write = __null_wr;
    devnull->ops.read_page = __null_rd_pg;
    devnull->ops.read = __null_rd;
}
EXPORT_PSEUDODEV(nulldev, pdev_nulldev_init);