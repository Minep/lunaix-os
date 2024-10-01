#ifndef __LUNAIX_AA64_ASM_H
#define __LUNAIX_AA64_ASM_H

#define __sr_encode(op0, op1, crn, crm, op2)    \
            s##op0##_##op1##_c##crn##_c##crm##_##op2

#ifndef __ASM__

#include <lunaix/compiler.h>

#else
#endif
#endif /* __LUNAIX_AA64_ASM_H */
