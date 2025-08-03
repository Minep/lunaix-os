#ifndef __LUNAIX_ARCH_GENERIC_ATOM_OPS
#define __LUNAIX_ARCH_GENERIC_ATOM_OPS


#ifndef atom_ldi
#define atom_ldi(ptr)	        (*(ptr))
#endif

#ifndef atom_ldl
#define atom_ldl(ptr)	        (*(ptr))
#endif


#ifndef atom_sti
#define atom_sti(ptr, v)        (*(ptr) = v)
#endif

#ifndef atom_stl
#define atom_stl(ptr, v)        (*(ptr) = v)
#endif


#ifndef atom_addi
#define atom_addi(ptr, v)       (*(ptr) += v) 
#endif

#ifndef atom_addl
#define atom_addl(ptr, v)       (*(ptr) += v) 
#endif


#ifndef atom_inci
#define atom_inci(ptr)          (*(ptr)++)
#endif

#ifndef atom_incl
#define atom_incl(ptr)          (*(ptr)++)
#endif


#ifndef atom_deci
#define atom_deci(ptr)          (*(ptr)--)
#endif

#ifndef atom_decl
#define atom_decl(ptr)          (*(ptr)--)
#endif

#ifndef atom_setf
#define atom_setf(ptr)          (*(ptr) = true)
#endif

#ifndef atom_clrf
#define atom_clrf(ptr)          (*(ptr) = false)
#endif


#endif
