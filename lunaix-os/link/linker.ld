ENTRY(start_)

SECTIONS {
    . = 0x100000;

    /* 这里是我们的高半核初始化代码段和数据段 */
    .boot.text BLOCK(4K) : {
        *(.multiboot)
        *(.boot.text)
    }

    .boot.bss BLOCK(4K) : {
        *(.boot.bss)
    }

    .boot.data BLOCK(4K) : {
        *(.boot.data)
    }

    .boot.rodata BLOCK(4K) : {
        *(.boot.rodata)
    }

    .boot.bss BLOCK(4K) : {
        *(.boot.rodata)
    }
    __kexec_boot_end = ALIGN(4K);

    /* ---- boot end ---- */

    /* ---- kernel start ---- */

    . += 0xC0000000;

    /* 好了，我们的内核…… */
    .text BLOCK(4K) : AT ( ADDR(.text) - 0xC0000000 ) {
        __kexec_start = .;
        PROVIDE(__kexec_text_start = .);
        
        *(.text)

        PROVIDE(__kexec_text_end = .);
    }

    .data BLOCK(4K) : AT ( ADDR(.data) - 0xC0000000 ) {
        *(.data)
    }

    .rodata BLOCK(4K) : AT ( ADDR(.rodata) - 0xC0000000 ) {
        *(.rodata)
    }

    .kpg BLOCK(4K) : AT ( ADDR(.kpg) - 0xC0000000 ) {
        *(.kpg)
    }

    . = ALIGN(4K);

    /* for generated array, we align to address line size */

    .lga BLOCK(4K) : AT ( ADDR(.lga) - 0xC0000000 ) {
        PROVIDE(__lga_twiplugin_inits_start = .);
        
        KEEP(*(.lga.twiplugin_inits));

        PROVIDE(__lga_twiplugin_inits_end = .);

        /* ---- */

        /* align to 8 bytes, so it can cover both 32 and 64 bits address line*/
        . = ALIGN(8);

        PROVIDE(__lga_pci_dev_drivers_start = .);
        
        KEEP(*(.lga.pci_dev_drivers));

        PROVIDE(__lga_pci_dev_drivers_end = .);

        /* ---- */

        . = ALIGN(8);

        PROVIDE(__lga_platdev_db_start = .);
        
        KEEP(*(.lga.platdev_db));

        PROVIDE(__lga_platdev_db_end = .);
    }

    .bss BLOCK(4K) : AT ( ADDR(.bss) - 0xC0000000 ) {
        *(.bss)
    }

    __kexec_end = ALIGN(4K);
}