#ifndef __LUNAIX_COMPILER_H
#define __LUNAIX_COMPILER_H

#define likely(x)               __builtin_expect(!!(x), 1)
#define unlikely(x)             __builtin_expect(!!(x), 0)

#define __section(name)         __attribute__((section(name)))
#define weak_alias(name)        __attribute__((weak, alias(name)))
#define optimize(opt)           __attribute__((optimize(opt)))
#define weak                    __attribute__((weak))
#define noret                   __attribute__((noreturn))
#define must_inline             __attribute__((always_inline))
#define must_emit               __attribute__((used))
#define unreachable             __builtin_unreachable()
#define no_inline               __attribute__((noinline))

#define defualt                 weak

#define clz(bits)               __builtin_clz(bits)
#define sadd_overflow(a, b, of) __builtin_sadd_overflow(a, b, of)
#define umul_overflow(a, b, of) __builtin_umul_overflow(a, b, of)
#define offsetof(f, m)          __builtin_offsetof(f, m)

#define prefetch_rd(ptr, ll)    __builtin_prefetch((ptr), 0, ll)
#define prefetch_wr(ptr, ll)    __builtin_prefetch((ptr), 1, ll)

#define stringify(v) #v
#define stringify__(v) stringify(v)

#define compact                 __attribute__((packed))
#define align(v)                __attribute__((aligned (v)))

#define cacheline_size          64
#define cacheline_align         align(cacheline_size)

#define export_symbol(domain, namespace, symbol)\
    typeof(symbol)* must_emit __SYMEXPORT_Z##domain##_N##namespace##_S##symbol \
         = &(symbol)

inline static void noret
spin()
{
    volatile int __infloop = 1;
    while (__infloop)
        ;
    unreachable;
}

#endif /* __LUNAIX_COMPILER_H */
