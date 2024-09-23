#ifndef __LUNAIX_PORT_IO_H
#define __LUNAIX_PORT_IO_H

#include <lunaix/types.h>

static inline u8_t
port_rdbyte(int port)
{
    u8_t data;
    asm volatile("inb %w1,%0" : "=a"(data) : "d"(port));
    return data;
}

static inline void
port_rdbytes(int port, void* addr, int cnt)
{
    asm volatile("cld\n"
                 "repne\n"
                 "insb"
                 : "=D"(addr), "=c"(cnt)
                 : "d"(port), "0"(addr), "1"(cnt)
                 : "memory", "cc");
}

static inline u16_t
port_rdword(int port)
{
    u16_t data;
    asm volatile("inw %w1,%0" : "=a"(data) : "d"(port));
    return data;
}

static inline void
port_rdwords(int port, void* addr, int cnt)
{
    asm volatile("cld\n"
                 "repne\n"
                 "insw"
                 : "=D"(addr), "=c"(cnt)
                 : "d"(port), "0"(addr), "1"(cnt)
                 : "memory", "cc");
}

static inline u32_t
port_rddword(int port)
{
    u32_t data;
    asm volatile("inl %w1,%0" : "=a"(data) : "d"(port));
    return data;
}

static inline void
port_rddwords(int port, void* addr, int cnt)
{
    asm volatile("cld\n"
                 "repne\n"
                 "insl"
                 : "=D"(addr), "=c"(cnt)
                 : "d"(port), "0"(addr), "1"(cnt)
                 : "memory", "cc");
}

static inline void
port_wrbyte(int port, u8_t data)
{
    asm volatile("outb %0, %w1" : : "a"(data), "d"(port));
}

static inline void
port_wrbytes(int port, const void* addr, int cnt)
{
    asm volatile("cld\n"
                 "repne\n"
                 "outsb"
                 : "=S"(addr), "=c"(cnt)
                 : "d"(port), "0"(addr), "1"(cnt)
                 : "cc");
}

static inline void
port_wrword(int port, u16_t data)
{
    asm volatile("outw %0,%w1" : : "a"(data), "d"(port));
}

static inline void
port_wrwords(int port, const void* addr, int cnt)
{
    asm volatile("cld\n"
                 "repne\n"
                 "outsw"
                 : "=S"(addr), "=c"(cnt)
                 : "d"(port), "0"(addr), "1"(cnt)
                 : "cc");
}

static inline void
port_wrdwords(int port, const void* addr, int cnt)
{
    asm volatile("cld\n"
                 "repne\n"
                 "outsl"
                 : "=S"(addr), "=c"(cnt)
                 : "d"(port), "0"(addr), "1"(cnt)
                 : "cc");
}

static inline void
port_wrdword(int port, u32_t data)
{
    asm volatile("outl %0,%w1" : : "a"(data), "d"(port));
}

static inline void
port_delay(int counter)
{
    asm volatile("   test %0, %0\n"
                 "   jz 1f\n"
                 "2: dec %0\n"
                 "   jnz 2b\n"
                 "1: dec %0" ::"a"(counter));
}

#endif /* __LUNAIX_PORT_port_H */
