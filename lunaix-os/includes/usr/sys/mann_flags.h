#ifndef __LUNAIX_SYS_MANN_FLAGS_H
#define __LUNAIX_SYS_MANN_FLAGS_H

// POSIX compliant.

// identity mapped to region attributes
#define PROT_READ (1 << 2)
#define PROT_WRITE (1 << 3)
#define PROT_EXEC (1 << 4)

// identity mapped to region attributes
#define MAP_WSHARED 0x2
#define MAP_RSHARED 0x1
#define MAP_SHARED (MAP_WSHARED | MAP_RSHARED)
#define MAP_PRIVATE 0x0
#define MAP_ANON (1 << 5)
#define MAP_STACK 0 // no effect in Lunaix
// other MAP_* goes should beyond 0x20

#define MS_ASYNC 0x1
#define MS_SYNC 0x2
#define MS_INVALIDATE 0x4

#endif /* __LUNAIX_MANN_FLAGS_H */
