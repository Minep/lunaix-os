#ifndef __LUNAIX_SPIKE_H
#define __LUNAIX_SPIKE_H

// Some helper functions. As helpful as Spike the Dragon! :)

// 除法 v/(2^k) 向上取整
#define CEIL(v, k) (((v) + (1 << (k)) - 1) >> (k))

#define ICEIL(x, y) ((x) / (y) + ((x) % (y) != 0))

// 除法 v/(2^k) 向下取整
#define FLOOR(v, k) ((v) >> (k))

// 获取v最近的最大k倍数
#define ROUNDUP(v, k) (((v) + (k)-1) & ~((k)-1))

// 获取v最近的最小k倍数 (k=2^m)
#define ROUNDDOWN(v, k) ((v) & ~((k)-1))

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/**
 * @brief Fast log base 2 for integer, utilizing constant unfolding.
 * Adopted from
 * https://elixir.bootlin.com/linux/v4.4/source/include/linux/log2.h#L85
 *
 */
#define ILOG2(x)                                                               \
    __builtin_constant_p(x) ? ((x) == 0              ? 0                       \
                               : ((x) & (1ul << 31)) ? 31                      \
                               : ((x) & (1ul << 30)) ? 30                      \
                               : ((x) & (1ul << 29)) ? 29                      \
                               : ((x) & (1ul << 28)) ? 28                      \
                               : ((x) & (1ul << 27)) ? 27                      \
                               : ((x) & (1ul << 26)) ? 26                      \
                               : ((x) & (1ul << 25)) ? 25                      \
                               : ((x) & (1ul << 24)) ? 24                      \
                               : ((x) & (1ul << 23)) ? 23                      \
                               : ((x) & (1ul << 22)) ? 22                      \
                               : ((x) & (1ul << 21)) ? 21                      \
                               : ((x) & (1ul << 20)) ? 20                      \
                               : ((x) & (1ul << 19)) ? 19                      \
                               : ((x) & (1ul << 18)) ? 18                      \
                               : ((x) & (1ul << 17)) ? 17                      \
                               : ((x) & (1ul << 16)) ? 16                      \
                               : ((x) & (1ul << 15)) ? 15                      \
                               : ((x) & (1ul << 14)) ? 14                      \
                               : ((x) & (1ul << 13)) ? 13                      \
                               : ((x) & (1ul << 12)) ? 12                      \
                               : ((x) & (1ul << 11)) ? 11                      \
                               : ((x) & (1ul << 10)) ? 10                      \
                               : ((x) & (1ul << 9))  ? 9                       \
                               : ((x) & (1ul << 8))  ? 8                       \
                               : ((x) & (1ul << 7))  ? 7                       \
                               : ((x) & (1ul << 6))  ? 6                       \
                               : ((x) & (1ul << 5))  ? 5                       \
                               : ((x) & (1ul << 4))  ? 4                       \
                               : ((x) & (1ul << 3))  ? 3                       \
                               : ((x) & (1ul << 2))  ? 2                       \
                               : ((x) & (1ul << 1))  ? 1                       \
                                                     : 0)                       \
                            : (31 - __builtin_clz(x))

#define __USER__ __attribute__((section(".usrtext")))

inline static void
spin()
{
    while (1)
        ;
}

#ifndef __LUNAIXOS_NASSERT__
#define assert(cond)                                                           \
    if (!(cond)) {                                                             \
        __assert_fail(#cond, __FILE__, __LINE__);                              \
    }

#define assert_msg(cond, msg)                                                  \
    if (!(cond)) {                                                             \
        __assert_fail(msg, __FILE__, __LINE__);                                \
    }
void
__assert_fail(const char* expr, const char* file, unsigned int line)
  __attribute__((noinline, noreturn));
#else
#define assert(cond) (void)(cond);          // assert nothing
#define assert_msg(cond, msg) (void)(cond); // assert nothing

#endif // __LUNAIXOS_NASSERT__

void
panick(const char* msg);

void
panickf(const char* fmt, ...);

#define wait_until(cond)                                                       \
    while (!(cond))                                                            \
        ;
#define loop_until(cond)                                                       \
    while (!(cond))                                                            \
        ;

#define wait_until_expire(cond, max)                                           \
    ({                                                                         \
        unsigned int __wcounter__ = (max);                                     \
        while (!(cond) && __wcounter__-- > 1)                                  \
            ;                                                                  \
        __wcounter__;                                                          \
    })

#endif /* __LUNAIX_SPIKE_H */
