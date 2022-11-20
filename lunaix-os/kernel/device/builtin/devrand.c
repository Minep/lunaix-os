#include <hal/rnd.h>
#include <lunaix/device.h>
#include <lunaix/syslog.h>

LOG_MODULE("rand")

int
__rand_rd_pg(struct device* dev, void* buf, size_t offset)
{
    rnd_fill(buf, PG_SIZE);
    return PG_SIZE;
}

int
__rand_rd(struct device* dev, void* buf, size_t offset, size_t len)
{
    rnd_fill(buf, len);
    return len;
}

void
devbuiltin_init_rand()
{
    if (!rnd_is_supported()) {
        kprintf(KWARN "not supported.\n");
        return;
    }
    struct device* devrand = device_addseq(NULL, NULL, "rand");
    devrand->read = __rand_rd;
    devrand->read_page = __rand_rd_pg;
}