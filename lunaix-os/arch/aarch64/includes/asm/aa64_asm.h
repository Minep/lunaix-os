#ifndef __LUNAIX_AA64_ASM_H
#define __LUNAIX_AA64_ASM_H

#include <lunaix/compiler.h>

#define __sr_encode(op0, op1, crn, crm, op2)    \
            s##op0##_##op1##_c##crn##_c##crm##_##op2

#endif /* __LUNAIX_AA64_ASM_H */
