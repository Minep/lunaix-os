#ifndef __LUNAIX_FLAGS_H
#define __LUNAIX_FLAGS_H

#if __ARCH__ == i386
#define PLATFORM_TARGET "x86_32"
#else
#define PLATFORM_TARGET "unknown"
#endif

#define LUNAIX_VER "0.0.1-dev"

/*
    Uncomment below to disable all assertion
*/
// #define __LUNAIXOS_NASSERT__

#endif /* __LUNAIX_FLAGS_H */
