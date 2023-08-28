/**
 * @file ldga.h
 * @author Lunaixsky (lunaxisky@qq.com)
 * @brief Linker generated array definition
 * @version 0.1
 * @date 2023-08-18
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef __LUNAIX_LDGA_H
#define __LUNAIX_LDGA_H

#include <lunaix/types.h>

#define ldga_el_id(ga_name, el_name) __lga_##ga_name##_##el_name
#define ldga_section(ga_name) __attribute__((section(".lga." ga_name)))

#define export_ldga_el(ga_name, el_name, type, val)                            \
    type ldga_section(#ga_name) ldga_el_id(ga_name, el_name) = (type)(val)

#define export_ldga_el_sfx(ga_name, el_name, type, val, suffix)                \
    type ldga_section(#ga_name "." #suffix) ldga_el_id(ga_name, el_name) =     \
      (type)(val)

#define export_ldga_el_idx(ga_name, i, type, val)                              \
    export_ldga_el(ga_name, i, type, val)
#define export_ldga_el_anon(ga_name, type, val)                                \
    export_ldga_el_idx(ga_name, __COUNTER__, type, val)

#define ldga_foreach(ga_name, el_type, index, el)                              \
    extern el_type __lga_##ga_name##_start[], __lga_##ga_name##_end;           \
    for (index = 0, el = __lga_##ga_name##_start[index];                       \
         (ptr_t)&__lga_##ga_name##_start[index] <                              \
         (ptr_t)&__lga_##ga_name##_end;                                        \
         el = __lga_##ga_name##_start[++index])

/**
 * @brief Invoke all elements in the array named `ga_name` of parameterless
 * function pointers
 *
 */
#define ldga_invoke_fn0(ga_name)                                               \
    ({                                                                         \
        int i = 0;                                                             \
        ptr_t fn0;                                                             \
        ldga_foreach(ga_name, ptr_t, i, fn0)                                   \
        {                                                                      \
            ((void (*)())fn0)();                                               \
        }                                                                      \
    })

#endif /* __LUNAIX_LDGA_H */
