#ifndef __LUNAIX_MEMPART_H
#define __LUNAIX_MEMPART_H

#ifdef CONFIG_ARCH_X86_64
#   include "mempart64.h"
#else
#   include "mempart32.h"
#endif

#endif