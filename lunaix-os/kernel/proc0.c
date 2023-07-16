#include <lunaix/block.h>
#include <lunaix/common.h>
#include <lunaix/exec.h>
#include <lunaix/foptions.h>
#include <lunaix/fs.h>
#include <lunaix/fs/probe_boot.h>
#include <lunaix/fs/twifs.h>
#include <lunaix/lxconsole.h>
#include <lunaix/mm/cake.h>
#include <lunaix/mm/pmm.h>
#include <lunaix/mm/valloc.h>
#include <lunaix/mm/vmm.h>
#include <lunaix/peripheral/ps2kbd.h>
#include <lunaix/peripheral/serial.h>
#include <lunaix/spike.h>
#include <lunaix/syscall.h>
#include <lunaix/syslog.h>
#include <lunaix/types.h>

#include <sdbg/protocol.h>

#include <hal/acpi/acpi.h>
#include <hal/ahci/ahci.h>
#include <hal/apic.h>
#include <hal/ioapic.h>
#include <hal/pci.h>
#include <hal/rtc.h>

#include <arch/x86/boot/multiboot.h>

#include <klibc/string.h>

LOG_MODULE("PROC0")

void
init_platform();

void
lock_reserved_memory();

void
unlock_reserved_memory();

void
__do_reserved_memory(int unlock);

int
mount_bootmedium()
{
    struct v_dnode* dnode;
    int errno = 0;
    struct device* dev = probe_boot_medium();
    if (!dev) {
        kprintf(KERROR "fail to acquire device. (%d)", errno);
        return 0;
    }

    if ((errno = vfs_mount("/mnt/lunaix-os", "iso9660", dev, 0))) {
        kprintf(KERROR "fail to mount boot medium. (%d)", errno);
        return 0;
    }

    return 1;
}

int
exec_initd()
{
    int errno = 0;

    if ((errno = exec_kexecve("/mnt/lunaix-os/usr/init", NULL, NULL))) {
        goto fail;
    }

    fail("should not reach");

fail:
    kprintf(KERROR "fail to load initd. (%d)", errno);
    return 0;
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

    if (!mount_bootmedium() || !exec_initd()) {
        while (1) {
            asm("hlt");
        }
        // should not reach
    }
}

extern uint8_t __kernel_start;            /* link/linker.ld */
extern uint8_t __kernel_end;              /* link/linker.ld */
extern uint8_t __init_hhk_end;            /* link/linker.ld */
extern multiboot_info_t* _k_init_mb_info; /* k_init.c */

void
init_platform()
{
    kprintf(KINFO "\033[11;0mLunaixOS (gcc v%s, %s)\033[39;49m\n",
            __VERSION__,
            __TIME__);

    // 锁定所有系统预留页（内存映射IO，ACPI之类的），并且进行1:1映射
    lock_reserved_memory();

    // firmware
    acpi_init(_k_init_mb_info);

    // die
    apic_init();
    ioapic_init();

    // debugger
    serial_init();
    sdbg_init();

    // timers & clock
    rtc_init();
    timer_init(SYS_TIMER_FREQUENCY_HZ);
    clock_init();

    // peripherals & chipset features
    ps2_kbd_init();
    block_init();
    ahci_init();

    pci_init();

    // console
    console_start_flushing();
    console_flush();

    // expose cake allocator states to vfs
    cake_export();

    unlock_reserved_memory();

    // clean up
    for (size_t i = 0; i < (uintptr_t)(&__init_hhk_end); i += PG_SIZE) {
        vmm_del_mapping(VMS_SELF, (void*)i);
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
            kprintf("mem: freeze: %p..%p type=%x\n",
                    pa,
                    pa + pg_num * PG_SIZE,
                    mmap.type);
            for (; j < pg_num; j++) {
                uintptr_t _pa = pa + (j << PG_SIZE_BITS);
                if (_pa >= KERNEL_MM_BASE) {
                    // Don't fuck up our kernel space!
                    break;
                }
                vmm_set_mapping(VMS_SELF, _pa, _pa, PG_PREM_R, VMAP_NULL);
                pmm_mark_page_occupied(
                  KERNEL_PID, _pa >> PG_SIZE_BITS, PP_FGLOCKED);
            }
            // Save the progress for later unmapping.
            mmaps[i].len_low = j * PG_SIZE;
        } else {
            kprintf("mem: reclaim: %p..%p type=%x\n",
                    pa,
                    pa + pg_num * PG_SIZE,
                    mmap.type);
            for (; j < pg_num; j++) {
                uintptr_t _pa = pa + (j << PG_SIZE_BITS);
                vmm_del_mapping(VMS_SELF, _pa);
                if (mmap.type == MULTIBOOT_MEMORY_ACPI_RECLAIMABLE) {
                    pmm_mark_page_free(_pa >> PG_SIZE_BITS);
                }
            }
        }
    }
}