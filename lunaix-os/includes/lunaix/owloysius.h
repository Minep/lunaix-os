#ifndef __LUNAIX_OWLOYSIUS_H
#define __LUNAIX_OWLOYSIUS_H

#include <lunaix/ds/ldga.h>

#define on_earlyboot c_earlyboot
#define on_boot c_boot
#define on_postboot c_postboot

#define owloysius_fetch_init(func, call_stage)                                     \
    export_ldga_el(lunainit, func, ptr_t, func);                            \
    export_ldga_el_sfx(lunainit, func##_##call_stage, ptr_t, func, call_stage);

#define invoke_init_function(stage) ldga_invoke_fn0(lunainit##_##stage)

#endif /* __LUNAIX_OWLOYSIUS_H */
