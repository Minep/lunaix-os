#ifndef __LUNAIX_AA64_ASM_H
#define __LUNAIX_AA64_ASM_H

#define __const_expr_sign #
#define __comma() ,
#define __const_expr()    __const_expr_sign

#define __sr_encode(op0, op1, crn, crm, op2)    \
            s##op0##_##op1##_c##crn##_c##crm##_##op2
#define __sysop_encode(op1, crn, crm, op2)      \
            "#" #op1 ",C" #crn ",C" #crm ",#" #op2

#ifndef __ASM__

#include <lunaix/compiler.h>

#else
#endif
#endif /* __LUNAIX_AA64_ASM_H */
