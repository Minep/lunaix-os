#ifndef __LUNAIX_COMPILER_H
#define __LUNAIX_COMPILER_H

#define likely(x) __builtin_expect((x), 1)

#define weak_alias(name) __attribute__((weak, alias(name)))

#endif /* __LUNAIX_COMPILER_H */
