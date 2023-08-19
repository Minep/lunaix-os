#ifndef __LUNAIX_COMPILER_H
#define __LUNAIX_COMPILER_H

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define weak_alias(name) __attribute__((weak, alias(name)))
#define noret __attribute__((noreturn))
#endif /* __LUNAIX_COMPILER_H */
