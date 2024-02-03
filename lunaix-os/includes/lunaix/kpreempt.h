#ifndef __LUNAIX_KPREEMPT_H
#define __LUNAIX_KPREEMPT_H

#include <sys/abi.h>

#define _preemptible __attribute__((section(".kf.preempt")))

#define ensure_preempt_caller()                                 \
    do {                                                        \
        extern int __kf_preempt_start[];                        \
        extern int __kf_preempt_end[];                          \
        ptr_t _retaddr = abi_get_retaddr();                     \
        assert_msg((ptr_t)__kf_preempt_start <= _retaddr        \
                    && _retaddr < (ptr_t)__kf_preempt_end,      \
                   "caller must be kernel preemptible");        \
    } while(0)

#endif /* __LUNAIX_KPREEMPT_H */
