#include <klibc/strfmt.h>
#include <lunaix/spike.h>
#include <lunaix/pcontext.h>

static char buffer[1024];

void noret
__assert_fail(const char* expr, const char* file, unsigned int line)
{
    ksprintf(buffer, "%s (%s:%u)", expr, file, line);

    // Here we load the buffer's address into %edi ("D" constraint)
    //  This is a convention we made that the LUNAIX_SYS_PANIC syscall will
    //  print the panic message passed via %edi. (see
    //  kernel/asm/x86/interrupts.c)
    cpu_trap_panic(buffer);

    spin(); // never reach
}

void noret
panick(const char* msg)
{
    cpu_trap_panic(msg);
    spin();
}

void
panickf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ksnprintfv(buffer, fmt, 1024, args);
    va_end(args);

    asm("int %0" ::"i"(LUNAIX_SYS_PANIC), "D"(buffer));
    spin();
}
