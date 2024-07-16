#include <lunaix/device.h>
#include <lunaix/mm/pagetable.h>

#include <klibc/string.h>

static inline void
rng_fill(void* data, size_t len)
{
#ifdef CONFIG_ARCH_X86_64
    asm volatile("1:\n"
                 "rdrand %%rax\n"
                 "movq   %%rax, (%0)\n"
                 "addq   $8, %%rax\n"
                 "subq   $8, %1\n"
                 "jnz    1b" 
                 ::
                 "r"((ptr_t)data),
                 "r"((len & ~0x7))
                 : 
                 "%eax");
#else
    asm volatile("1:\n"
                 "rdrand %%eax\n"
                 "movl %%eax, (%0)\n"
                 "addl $4, %%eax\n"
                 "subl $4, %1\n"
                 "jnz 1b" 
                 ::
                 "r"((ptr_t)data),
                 "r"((len & ~0x3))
                 : 
                 "%eax");
#endif
}

static int
__rand_rd_pg(struct device* dev, void* buf, size_t offset)
{
    rng_fill(buf, PAGE_SIZE);
    return PAGE_SIZE;
}

static int
__rand_rd(struct device* dev, void* buf, size_t offset, size_t len)
{
    if (unlikely(len < sizeof(ptr_t))) {
        int tmp_buf = 0;
        rng_fill(&tmp_buf, sizeof(ptr_t));
        memcpy(buf, &tmp_buf, len);
    } else {
        rng_fill(buf, len);
    }
    return len;
}

int
pdev_randdev_init(struct device_def* devdef)
{
    // FIXME add check on cpuid for presence of rdrand
    struct device* devrand = device_allocseq(NULL, NULL);
    devrand->ops.read = __rand_rd;
    devrand->ops.read_page = __rand_rd_pg;

    register_device(devrand, &devdef->class, "rand");

    return 0;
}

static struct device_def devrandx86_def = {
    .name = "x86 On-Chip RNG",
    .class = DEVCLASS(DEVIF_SOC, DEVFN_CHAR, DEV_RNG),
    .init = pdev_randdev_init};
EXPORT_DEVICE(randdev, &devrandx86_def, load_onboot);