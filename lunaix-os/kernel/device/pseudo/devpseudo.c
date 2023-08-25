#include <lunaix/device.h>

extern void
devbuiltin_init_rand();

extern void
devbuiltin_init_null();

void
device_install_pseudo()
{
    ptr_t pdev_init_fn;
    int index;
    ldga_foreach(pseudo_dev, ptr_t, index, pdev_init_fn)
    {
        ((void (*)())pdev_init_fn)();
    }
}