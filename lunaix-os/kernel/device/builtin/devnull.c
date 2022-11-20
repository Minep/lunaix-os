#include <lunaix/device.h>

int
__null_wr_pg(struct device* dev, void* buf, size_t offset)
{
    // do nothing
    return PG_SIZE;
}

int
__null_wr(struct device* dev, void* buf, size_t offset, size_t len)
{
    // do nothing
    return len;
}

int
__null_rd_pg(struct device* dev, void* buf, size_t offset)
{
    // do nothing
    return 0;
}

int
__null_rd(struct device* dev, void* buf, size_t offset, size_t len)
{
    // do nothing
    return 0;
}

void
devbuiltin_init_null()
{
    struct device* devnull = device_addseq(NULL, NULL, "null");
    devnull->write_page = __null_wr_pg;
    devnull->write = __null_wr;
    devnull->read_page = __null_rd_pg;
    devnull->read = __null_rd;
}