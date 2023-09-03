#include <lunaix/device.h>
#include <lunaix/mm/page.h>

static inline void
rng_fill(void* data, size_t len)
{
    asm volatile("1:\n"
                 "rdrand %%eax\n"
                 "movl %%eax, (%0)\n"
                 "addl $4, %%eax\n"
                 "subl $4, %1\n"
                 "jnz 1b" ::"r"((ptr_t)data),
                 "r"((len & ~0x3))
                 : "%eax");
}

static int
__rand_rd_pg(struct device* dev, void* buf, size_t offset)
{
    rng_fill(buf, PG_SIZE);
    return PG_SIZE;
}

static int
__rand_rd(struct device* dev, void* buf, size_t offset, size_t len)
{
    rng_fill(buf, len);
    return len;
}

int
pdev_randdev_init(struct device_def* devdef)
{
    struct device* devrand = device_addseq(NULL, NULL, "rand");
    devrand->ops.read = __rand_rd;
    devrand->ops.read_page = __rand_rd_pg;

    return 0;
}

static struct device_def devrandx86_def = {
    .name = "x86 On-Chip RNG",
    .class = DEVCLASS(DEVIF_SOC, DEVFN_CHAR, DEV_RNG, 0),
    .init = pdev_randdev_init
};
EXPORT_DEVICE(randdev, &devrandx86_def, load_earlystage);