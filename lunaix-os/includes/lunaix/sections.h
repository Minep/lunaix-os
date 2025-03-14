#ifndef __LUNAIX_SECTIONS_H
#define __LUNAIX_SECTIONS_H

#include <lunaix/types.h>

#define __mark_name(n, s)   __##n##_##s
#define __section_mark(name, suffix)    \
    ({ extern unsigned long __mark_name(name,suffix)[]; \
       __ptr(__mark_name(name,suffix)); })


/*  Auto-generated data  */

#define extern_autogen(name)                                         \
            _weak unsigned long __mark_name(autogen,name)[] = {};     \
            extern unsigned long __mark_name(autogen,name)[];

#define autogen_name(name)  __mark_name(autogen,name)

#define autogen(type, name)     \
            ((type*)__mark_name(autogen,name))


/*  Common section definitions  */

#define reclaimable         __section(".bss.reclaim")
#define reclaimable_start   __section_mark(bssreclaim, start)
#define reclaimable_end     __section_mark(bssreclaim, end)

#define bootsec_start       __section_mark(kboot, start)
#define bootsec_end         __section_mark(kboot, end)

#define kernel_start        __section_mark(kexec, start)
#define kernel_load_end     __section_mark(kexec, end)
#define kernel_end          __section_mark(kimg, end)

#ifdef CONFIG_USE_DEVICETREE
#define dtb_start           __section_mark(dtb, start)
#endif


/*  kernel section mapping info  */

struct ksection
{
    ptr_t va;
    ptr_t pa;
    unsigned int size;
    unsigned int flags;
} align(4);

struct ksecmap
{
    unsigned int num;
    unsigned int ksize;
    struct ksection secs[0];
} align(4);

#endif /* __LUNAIX_SECTIONS_H */
