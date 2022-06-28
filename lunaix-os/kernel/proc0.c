#include <arch/x86/boot/multiboot.h>
#include <lunaix/common.h>
#include <lunaix/lunistd.h>
#include <lunaix/lxconsole.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/peripheral/ps2kbd.h>
#include <lunaix/proc.h>
#include <lunaix/spike.h>
#include <lunaix/syscall.h>
#include <lunaix/syslog.h>
#include <stddef.h>

#include <hal/acpi/acpi.h>
#include <hal/ahci.h>
#include <hal/apic.h>
#include <hal/ioapic.h>
#include <hal/pci.h>

LOG_MODULE("PROC0")

extern void
_lxinit_main(); /* lxinit.c */

void
init_platform();

void
lock_reserved_memory();

void
unlock_reserved_memory();

void
__do_reserved_memory(int unlock);

//#define USE_DEMO
#define DEMO_SIGNAL

extern void
_pconsole_main();

extern void
_signal_demo_main();

extern void
_lxinit_main();

void __USER__
__proc0_usr()
{
    pid_t p;
    if (!fork()) {
        _pconsole_main();
    }

    if (!(p = fork())) {
#ifndef USE_DEMO
        _exit(0);
#elif defined DEMO_SIGNAL
        _signal_demo_main();
#else
        _lxinit_main();
#endif
    }

    waitpid(p, 0, 0);

    while (1) {
        yield();
    }
}

/**
 * @brief LunaixOS的零号进程，该进程永远为可执行。
 *
 * 这主要是为了保证调度器在没有进程可调度时依然有事可做。
 *
 * 同时，该进程也负责fork出我们的init进程。
 *
 */
void
__proc0()
{
    init_platform();

    init_proc_user_space(__current);

    asm volatile("movw %0, %%ax\n"
                 "movw %%ax, %%es\n"
                 "movw %%ax, %%ds\n"
                 "movw %%ax, %%fs\n"
                 "movw %%ax, %%gs\n"
                 "pushl %0\n"
                 "pushl %1\n"
                 "pushl %2\n"
                 "pushl %3\n"
                 "retf" ::"i"(UDATA_SEG),
                 "i"(USTACK_TOP & ~0xf),
                 "i"(UCODE_SEG),
                 "r"(__proc0_usr)
                 : "eax", "memory");
}

extern uint8_t __kernel_start;            /* link/linker.ld */
extern uint8_t __kernel_end;              /* link/linker.ld */
extern uint8_t __init_hhk_end;            /* link/linker.ld */
extern multiboot_info_t* _k_init_mb_info; /* k_init.c */

void
init_platform()
{
    assert_msg(kalloc_init(), "Fail to initialize heap");

    // 锁定所有系统预留页（内存映射IO，ACPI之类的），并且进行1:1映射
    lock_reserved_memory();

    acpi_init(_k_init_mb_info);
    apic_init();
    ioapic_init();
    timer_init(SYS_TIMER_FREQUENCY_HZ);
    clock_init();
    ps2_kbd_init();
    pci_init();
    ahci_init();
    pci_print_device();

    syscall_install();

    console_start_flushing();
    console_flush();

    unlock_reserved_memory();

    for (size_t i = 0; i < (uintptr_t)(&__init_hhk_end); i += PG_SIZE) {
        vmm_del_mapping(PD_REFERENCED, (void*)i);
        pmm_free_page(KERNEL_PID, (void*)i);
    }
}

void
lock_reserved_memory()
{
    __do_reserved_memory(0);
}

void
unlock_reserved_memory()
{
    __do_reserved_memory(1);
}

void
__do_reserved_memory(int unlock)
{
    multiboot_memory_map_t* mmaps = _k_init_mb_info->mmap_addr;
    size_t map_size =
      _k_init_mb_info->mmap_length / sizeof(multiboot_memory_map_t);
    // v_mapping mapping;
    for (unsigned int i = 0; i < map_size; i++) {
        multiboot_memory_map_t mmap = mmaps[i];
        uint8_t* pa = PG_ALIGN(mmap.addr_low);
        if (mmap.type == MULTIBOOT_MEMORY_AVAILABLE || pa <= MEM_4MB) {
            // Don't fuck up our kernel code or any free area!
            continue;
        }
        size_t pg_num = CEIL(mmap.len_low, PG_SIZE_BITS);
        size_t j = 0;
        if (!unlock) {
            for (; j < pg_num; j++) {
                uintptr_t _pa = pa + (j << PG_SIZE_BITS);
                if (_pa >= KERNEL_MM_BASE) {
                    // Don't fuck up our kernel space!
                    break;
                }
                vmm_set_mapping(PD_REFERENCED, _pa, _pa, PG_PREM_R, VMAP_NULL);
            }
            // Save the progress for later unmapping.
            mmaps[i].len_low = j * PG_SIZE;
        } else {
            for (; j < pg_num; j++) {
                uintptr_t _pa = pa + (j << PG_SIZE_BITS);
                vmm_del_mapping(PD_REFERENCED, _pa);
            }
        }
    }
}