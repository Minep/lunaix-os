#include <sys/interrupts.h>
#include <klibc/stdio.h>
#include <lunaix/spike.h>

static char buffer[1024];

void
__assert_fail(const char* expr, const char* file, unsigned int line)
{
    ksprintf(buffer, "%s (%s:%u)", expr, file, line);

    // Here we load the buffer's address into %edi ("D" constraint)
    //  This is a convention we made that the LUNAIX_SYS_PANIC syscall will
    //  print the panic message passed via %edi. (see
    //  kernel/asm/x86/interrupts.c)
    cpu_trap_panic(buffer);

    DO_SPIN // never reach
}

void
panick(const char* msg)
{       
    cpu_trap_panic(msg);
    DO_SPIN
}

void
panickf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    __ksprintf_internal(buffer, fmt, 1024, args);
    va_end(args);

    asm("int %0" ::"i"(LUNAIX_SYS_PANIC), "D"(buffer));
    DO_SPIN
}
