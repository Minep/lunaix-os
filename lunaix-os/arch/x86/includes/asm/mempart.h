#ifndef __LUNAIX_MEMPART_H
#define __LUNAIX_MEMPART_H

#ifdef CONFIG_ARCH_X86_64
#   include "variants/mempart64.h"
#else
#   include "variants/mempart32.h"
#endif

#endif