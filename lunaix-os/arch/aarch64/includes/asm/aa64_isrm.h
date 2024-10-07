#ifndef __LUNAIX_AA64_ISRM_H
#define __LUNAIX_AA64_ISRM_H

#include <asm-generic/isrm.h>
#include "soc/gic.h"

unsigned int
aa64_isrm_ivalloc(struct gic_int_param* ivcfg, isr_cb handler);

#endif /* __LUNAIX_AA64_ISRM_H */
