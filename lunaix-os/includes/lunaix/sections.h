#ifndef __LUNAIX_SECTIONS_H
#define __LUNAIX_SECTIONS_H

#include <lunaix/types.h>

#define __mark_name(n, s)   __##n##_##s
#define __section_mark(name, suffix)    \
    ({ extern char __mark_name(name,suffix)[]; \
       __ptr(__mark_name(name,suffix)); })

#define reclaimable         __section(".bss.reclaim")
#define reclaimable_start   __section_mark(bssreclaim, start)
#define reclaimable_end     __section_mark(bssreclaim, end)

#define kernel_start        __section_mark(kexec, start)
#define kernel_load_end     __section_mark(kexec, end)
#define kernel_end          __section_mark(kimg, end)

#ifdef CONFIG_USE_DEVICETREE
#define dtb_start           __section_mark(dtb, start)
#endif

#endif /* __LUNAIX_SECTIONS_H */
