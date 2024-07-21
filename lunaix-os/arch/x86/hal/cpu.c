#include <cpuid.h>
#include <lunaix/types.h>
#include <sys/cpu.h>
#include <sys/vectors.h>

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
