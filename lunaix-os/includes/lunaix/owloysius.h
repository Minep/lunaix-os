#ifndef __LUNAIX_OWLOYSIUS_H
#define __LUNAIX_OWLOYSIUS_H

#include <lunaix/ds/ldga.h>

#define call_on_earlyboot c_earlyboot
#define call_on_boot c_boot
#define call_on_postboot c_postboot

#define lunaix_initfn(func, call_stage)                                     \
    export_ldga_el(lunainit, func, ptr_t, func);                            \
    export_ldga_el_sfx(lunainit, func##_##call_stage, ptr_t, func, call_stage);

#define invoke_init_function(stage) ldga_invoke_fn0(lunainit##_##stage)

#endif /* __LUNAIX_OWLOYSIUS_H */
