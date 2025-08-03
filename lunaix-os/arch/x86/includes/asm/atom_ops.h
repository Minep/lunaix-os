#ifndef __LUNAIX_ATOM_OPS_H
#define __LUNAIX_ATOM_OPS_H

#include <lunaix/compiler.h>
#include <lunaix/types.h>

#define __atom_inc(sfx, opnd) \
    asm volatile ("lock inc" #sfx " %0" : "+m"(*(opnd)))

#define __atom_dec(sfx, opnd) \
    asm volatile ("lock dec" #sfx " %0" : "+m"(*(opnd)))

#define __atom_xchg(sfx, m_opnd1, opnd2) \
    asm volatile ("xchg" #sfx " %0, %1" : "+r"(opnd2), "+m"(*(m_opnd1)))

#define __atom_add(sfx, result, opnd1) \
    asm volatile ("lock add" #sfx " %1, %0" : "+m"(*(result)) : "ir"(opnd1))

#define ___atom_ld(sfx, from_mem, to) \
    asm volatile ("mov" #sfx " %1, %0" : "=r"(to) : "m"(*(from_mem)))


#define __atom_ld(type, sfx, ptr) \
	    ({ type _v = 0; ___atom_ld(sfx, ptr, _v); _v; })

#define __atom_st(type, sfx, ptr, v) \
	    ({ type _v = v; __atom_xchg(sfx, ptr, _v); _v; })

#define atom_ldi(ptr)	        __atom_ld(u32_t, l, ptr)
#define atom_ldl(ptr)	        __atom_ld(u64_t, q, ptr)

#define atom_sti(ptr, v)        __atom_st(u32_t, l, ptr, v)
#define atom_stl(ptr, v)        __atom_st(u64_t, q, ptr, v)

#define atom_addi(ptr, v)       __atom_add(l, ptr, v)
#define atom_addl(ptr, v)       __atom_add(q, ptr, v)

#define atom_inci(ptr)          __atom_inc(l, ptr)
#define atom_incl(ptr)          __atom_inc(q, ptr)

#define atom_deci(ptr)          __atom_dec(l, ptr)
#define atom_decl(ptr)          __atom_dec(q, ptr)

#define atom_setf(ptr)          __atom_st(bool, l, ptr, true)
#define atom_clrf(ptr)          __atom_st(bool, l, ptr, false)

#endif /* __LUNAIX_ATOM_OPS_H */
