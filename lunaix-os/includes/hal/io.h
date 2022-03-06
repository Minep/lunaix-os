#ifndef __LUNAIX_IO_H
#define __LUNAIX_IO_H

#include <stdint.h>

static inline uint8_t
io_inb(int port)
{
    uint8_t data;
    asm volatile("inb %w1,%0" : "=a"(data) : "d"(port));
    return data;
}

static inline void
io_insb(int port, void* addr, int cnt)
{
    asm volatile("cld\n"
                 "repne\n"
                 "insb"
                 : "=D"(addr), "=c"(cnt)
                 : "d"(port), "0"(addr), "1"(cnt)
                 : "memory", "cc");
}

static inline uint16_t
io_inw(int port)
{
    uint16_t data;
    asm volatile("inw %w1,%0" : "=a"(data) : "d"(port));
    return data;
}

static inline void
io_insw(int port, void* addr, int cnt)
{
    asm volatile("cld\n"
                 "repne\n"
                 "insw"
                 : "=D"(addr), "=c"(cnt)
                 : "d"(port), "0"(addr), "1"(cnt)
                 : "memory", "cc");
}

static inline uint32_t
io_inl(int port)
{
    uint32_t data;
    asm volatile("inl %w1,%0" : "=a"(data) : "d"(port));
    return data;
}

static inline void
io_insl(int port, void* addr, int cnt)
{
    asm volatile("cld\n"
                 "repne\n"
                 "insl"
                 : "=D"(addr), "=c"(cnt)
                 : "d"(port), "0"(addr), "1"(cnt)
                 : "memory", "cc");
}

static inline void
io_outb(int port, uint8_t data)
{
    asm volatile("outb %0, %w1" : : "a"(data), "d"(port));
}

static inline void
io_outsb(int port, const void* addr, int cnt)
{
    asm volatile("cld\n"
                 "repne\n"
                 "outsb"
                 : "=S"(addr), "=c"(cnt)
                 : "d"(port), "0"(addr), "1"(cnt)
                 : "cc");
}

static inline void
io_outw(int port, uint16_t data)
{
    asm volatile("outw %0,%w1" : : "a"(data), "d"(port));
}

static inline void
io_outsw(int port, const void* addr, int cnt)
{
    asm volatile("cld\n"
                 "repne\n"
                 "outsw"
                 : "=S"(addr), "=c"(cnt)
                 : "d"(port), "0"(addr), "1"(cnt)
                 : "cc");
}

static inline void
io_outsl(int port, const void* addr, int cnt)
{
    asm volatile("cld\n"
                 "repne\n"
                 "outsl"
                 : "=S"(addr), "=c"(cnt)
                 : "d"(port), "0"(addr), "1"(cnt)
                 : "cc");
}

static inline void
io_outl(int port, uint32_t data)
{
    asm volatile("outl %0,%w1" : : "a"(data), "d"(port));
}

#endif /* __LUNAIX_IO_H */
