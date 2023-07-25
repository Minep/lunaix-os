ENTRY(start_)

/*
    FUTURE: Use disk reader
    A bit of messy here.
    We will pull our higher half kernel out of this shit
      and load it separately once we have our disk reader.
*/

SECTIONS {
    . = 0x100000;

    /* 这里是我们的高半核初始化代码段和数据段 */
    .hhk_init_text BLOCK(4K) : {
        *(.multiboot)
        *(.hhk_init_text)
    }

    .hhk_init_bss BLOCK(4K) : {
        *(.hhk_init_bss)
    }

    .hhk_init_data BLOCK(4K) : {
        *(.hhk_init_data)
    }

    .hhk_init_rodata BLOCK(4K) : {
        *(.hhk_init_rodata)
    }
    __init_hhk_end = ALIGN(4K);

    /* Relocation of our higher half kernel */
    . += 0xC0000000;

    /* 好了，我们的内核…… */
    .text BLOCK(4K) : AT ( ADDR(.text) - 0xC0000000 ) {
        __kernel_start = .;
        PROVIDE(__ktext_start = .);
        
        *(.text)

        PROVIDE(__ktext_end = .);
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

    .bss BLOCK(4K) : AT ( ADDR(.bss) - 0xC0000000 ) {
        *(.bss)
    }

    __kernel_end = ALIGN(4K);
}