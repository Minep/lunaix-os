#ifndef __LUNAIX_COMPILER_H
#define __LUNAIX_COMPILER_H

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define weak_alias(name) __attribute__((weak, alias(name)))
#define weak __attribute__((weak))
#define noret __attribute__((noreturn))

#define stringify(v) #v
#define stringify__(v) stringify(v)

#endif /* __LUNAIX_COMPILER_H */
