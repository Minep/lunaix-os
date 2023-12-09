#ifndef __LUNAIX_COMPILER_H
#define __LUNAIX_COMPILER_H

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define weak_alias(name) __attribute__((weak, alias(name)))
#define weak __attribute__((weak))
#define noret __attribute__((noreturn))
#define optimize(opt) __attribute__((optimize(opt)))
#define must_inline __attribute__((always_inline))

#define clz(bits) __builtin_clz(bits)
#define sadd_overflow(a, b, of) __builtin_sadd_overflow(a, b, of)
#define umul_overflow(a, b, of) __builtin_umul_overflow(a, b, of)
#define offsetof(f, m) __builtin_offsetof(f, m)

#define prefetch_rd(ptr, ll) __builtin_prefetch((ptr), 0, ll)
#define prefetch_wr(ptr, ll) __builtin_prefetch((ptr), 1, ll)

#define stringify(v) #v
#define stringify__(v) stringify(v)

#define compact __attribute__((packed))
#define align(v) __attribute__((aligned (v)))

inline static void noret
spin()
{
    volatile int __infloop = 1;
    while (__infloop)
        ;
    __builtin_unreachable();
}

#endif /* __LUNAIX_COMPILER_H */
