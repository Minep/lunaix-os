#include <arch/i386/vectors.h>
#include <cpuid.h>
#include <hal/cpu.h>
#include <lunaix/types.h>

#define BRAND_LEAF 0x80000000UL

void
cpu_get_model(char* model_out)
{
    u32_t* out = (u32_t*)model_out;
    u32_t eax = 0, ebx = 0, edx = 0, ecx = 0;

    __get_cpuid(0, &eax, &ebx, &ecx, &edx);

    out[0] = ebx;
    out[1] = edx;
    out[2] = ecx;
    model_out[12] = '\0';
}

int
cpu_brand_string_supported()
{
    u32_t supported = __get_cpuid_max(BRAND_LEAF, 0);
    return (supported >= 0x80000004UL);
}

void
cpu_get_id(char* id_out)
{
    if (!cpu_brand_string_supported()) {
        id_out[0] = '?';
        id_out[1] = '\0';
    }
    u32_t* out = (u32_t*)id_out;
    u32_t eax = 0, ebx = 0, edx = 0, ecx = 0;
    for (u32_t i = 2, j = 0; i < 5; i++) {
        __get_cpuid(BRAND_LEAF + i, &eax, &ebx, &ecx, &edx);
        out[j] = eax;
        out[j + 1] = ebx;
        out[j + 2] = ecx;
        out[j + 3] = edx;
        j += 4;
    }
    id_out[48] = '\0';
}

u32_t
cpu_ldstate()
{
    ptr_t val;
    asm volatile("pushf\n"
                 "popl %0\n"
                 : "=r"(val)::);
    return val;
}

u32_t
cpu_ldconfig()
{
    ptr_t val;
    asm volatile("movl %%cr0,%0" : "=r"(val));
    return val;
}

void
cpu_chconfig(u32_t val)
{
    asm("mov %0, %%cr0" ::"r"(val));
}

u32_t
cpu_ldvmspace()
{
    ptr_t val;
    asm volatile("movl %%cr3,%0" : "=r"(val));
    return val;
}

void
cpu_chvmspace(u32_t val)
{
    asm("mov %0, %%cr3" ::"r"(val));
}

void
cpu_flush_page(ptr_t va)
{
    asm volatile("invlpg (%0)" ::"r"(va) : "memory");
}

void
cpu_flush_vmspace()
{
    asm("movl %%cr3, %%eax\n"
        "movl %%eax, %%cr3" ::
          : "eax");
}

void
cpu_enable_interrupt()
{
    asm volatile("sti");
}

void
cpu_disable_interrupt()
{
    asm volatile("cli");
}

void
cpu_trap_sched()
{
    asm("int %0" ::"i"(LUNAIX_SCHED));
}

void
cpu_trap_panic(char* message)
{
    asm("int %0" ::"i"(LUNAIX_SYS_PANIC), "D"(message));
}

void
cpu_wait()
{
    asm("hlt");
}

ptr_t
cpu_ldeaddr()
{
    ptr_t val;
    asm volatile("movl %%cr2,%0" : "=r"(val));
    return val;
}

void
cpu_rdmsr(u32_t msr_idx, u32_t* reg_high, u32_t* reg_low)
{
    u32_t h = 0, l = 0;
    asm volatile("rdmsr" : "=d"(h), "=a"(l) : "c"(msr_idx));

    *reg_high = h;
    *reg_low = l;
}

void
cpu_wrmsr(u32_t msr_idx, u32_t reg_high, u32_t reg_low)
{
    asm volatile("wrmsr" : : "d"(reg_high), "a"(reg_low), "c"(msr_idx));
}