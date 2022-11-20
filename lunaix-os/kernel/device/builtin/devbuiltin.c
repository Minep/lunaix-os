#include <lunaix/device.h>

extern void
devbuiltin_init_rand();

extern void
devbuiltin_init_null();

void
device_init_builtin()
{
    devbuiltin_init_null();
    devbuiltin_init_rand();
}