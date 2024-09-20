#ifndef __LUNAIX_ARCH_PORT_IO_H
#define __LUNAIX_ARCH_PORT_IO_H

#include <lunaix/types.h>

static inline u8_t
port_rdbyte(int port)
{
    return 0;
}

static inline void
port_rdbytes(int port, void* addr, int cnt)
{
    return;
}

static inline u16_t
port_rdword(int port)
{
    return 0;
}

static inline void
port_rdwords(int port, void* addr, int cnt)
{
    return;
}

static inline u32_t
port_rddword(int port)
{
    return 0;
}

static inline void
port_rddwords(int port, void* addr, int cnt)
{
    return;
}

static inline void
port_wrbyte(int port, u8_t data)
{
    return;
}

static inline void
port_wrbytes(int port, const void* addr, int cnt)
{
    return;
}

static inline void
port_wrword(int port, u16_t data)
{
    return;
}

static inline void
port_wrwords(int port, const void* addr, int cnt)
{
    return;
}

static inline void
port_wrdwords(int port, const void* addr, int cnt)
{
    return;
}

static inline void
port_wrdword(int port, u32_t data)
{
    return;
}

static inline void
port_delay(int counter)
{
    return;
}

#endif /* __LUNAIX_ARCH_PORT_IO_H */
