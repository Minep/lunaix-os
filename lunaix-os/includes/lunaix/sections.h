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

#endif /* __LUNAIX_SECTIONS_H */
