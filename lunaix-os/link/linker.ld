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

        PROVIDE(__lga_devdefs_start = .);
        
        KEEP(*(.lga.devdefs));

        PROVIDE(__lga_devdefs_end = .);

        /* ---- */

        . = ALIGN(8);

        PROVIDE(__lga_dev_ld_kboot_start = .);
        
        KEEP(*(.lga.devdefs.ld_kboot));

        PROVIDE(__lga_dev_ld_kboot_end = .);

        /* ---- */

        . = ALIGN(8);

        PROVIDE(__lga_dev_ld_sysconf_start = .);
        
        KEEP(*(.lga.devdefs.ld_sysconf));

        PROVIDE(__lga_dev_ld_sysconf_end = .);

        /* ---- */

        . = ALIGN(8);

        PROVIDE(__lga_dev_ld_timedev_start = .);
        
        KEEP(*(.lga.devdefs.ld_timedev));

        PROVIDE(__lga_dev_ld_timedev_end = .);

        /* ---- */

        . = ALIGN(8);

        PROVIDE(__lga_dev_ld_post_start = .);
        
        KEEP(*(.lga.devdefs.ld_post));

        PROVIDE(__lga_dev_ld_post_end = .);

        /* ---- */

        . = ALIGN(8);

        PROVIDE(__lga_fs_start = .);
        
        KEEP(*(.lga.fs));

        PROVIDE(__lga_fs_end = .);

        /* ---- */

        . = ALIGN(8);

        PROVIDE(__lga_lunainit_call_on_boot_start = .);
        
        KEEP(*(.lga.lunainit.c_boot));

        PROVIDE(__lga_lunainit_call_on_boot_end = .);

        /* ---- */

        . = ALIGN(8);

        PROVIDE(__lga_lunainit_call_on_postboot_start = .);
        
        KEEP(*(.lga.lunainit.c_postboot));

        PROVIDE(__lga_lunainit_call_on_postboot_end = .);

    }

    .bss BLOCK(4K) : AT ( ADDR(.bss) - 0xC0000000 ) {
        *(.bss)
    }

    __kexec_end = ALIGN(4K);
}