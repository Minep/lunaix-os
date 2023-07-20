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
        * (.multiboot)
        arch/*.o (.hhk_init)
        arch/*.o (.text)
    }

    .hhk_init_bss BLOCK(4K) : {
        arch/*.o (.bss)
    }

    .hhk_init_data BLOCK(4K) : {
        arch/*.o (.data)
    }

    .hhk_init_rodata BLOCK(4K) : {
        arch/*.o (.rodata)
    }
    __init_hhk_end = ALIGN(4K);

    /* Relocation of our higher half kernel */
    . += 0xC0000000;

    /* 好了，我们的内核…… */
    .text BLOCK(4K) : AT ( ADDR(.text) - 0xC0000000 ) {
        __kernel_start = .;
        kernel/*.o (.text)
        hal/*.o (.text)
        debug/*.o (.text)
        libs/*.o (.text)
    }

    __usrtext_start = ALIGN(4K);
    .usrtext BLOCK(4K) : AT ( ADDR(.usrtext) - 0xC0000000 ) {
        * (.usrtext)
    }
    __usrtext_end = ALIGN(4K);

    .data BLOCK(4K) : AT ( ADDR(.data) - 0xC0000000 ) {
        kernel/*.o (.data)
        hal/*.o (.data)
        debug/*.o (.data)
        libs/*.o (.data)
    }

    .rodata BLOCK(4K) : AT ( ADDR(.rodata) - 0xC0000000 ) {
        kernel/*.o (.rodata)
        hal/*.o (.rodata)
        debug/*.o (.rodata)
        libs/*.o (.rodata)
    }

    .kpg BLOCK(4K) : AT ( ADDR(.kpg) - 0xC0000000 ) {
        arch/*.o (.kpg)
    }

    .bss BLOCK(4K) : AT ( ADDR(.bss) - 0xC0000000 ) {
        kernel/*.o (.bss)
        hal/*.o (.bss)
        debug/*.o (.bss)
        libs/*.o (.bss)
    }

    __kernel_end = ALIGN(4K);
}