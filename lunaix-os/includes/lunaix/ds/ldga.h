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

#define export_ldga_el(ga_name, el_name, type, val)                            \
    type __attribute__((section(".lga." #ga_name)))                            \
    __lga_##ga_name##_##el_name = (type)(val)

#define ldga_foreach(ga_name, el_type, index, el)                              \
    extern el_type __lga_##ga_name##_start[], __lga_##ga_name##_end;           \
    for (index = 0, el = __lga_##ga_name##_start[index];                       \
         (ptr_t)&__lga_##ga_name##_start[index] <                              \
         (ptr_t)&__lga_##ga_name##_end;                                        \
         el = __lga_##ga_name##_start[++index])

#endif /* __LUNAIX_LDGA_H */
