ENTRY(start_)

/*
    FUTURE: Use dynamic linker to separate kernel and boot code
    A bit of messy here.
    We will pull our higher half kernel out of this shit
      and load it separately once we have our dynamic linker ready.
*/

SECTIONS {
    . = 0x100000;

    /* 这里是我们的高半核初始化代码段和数据段 */

    .hhk_init_text BLOCK(4K) : {
        * (.multiboot)
        build/obj/arch/x86/*.o (.hhk_init)
        build/obj/arch/x86/*.o (.text)
    }

    .hhk_init_bss BLOCK(4K) : {
        build/obj/arch/x86/*.o (.bss)
    }

    .hhk_init_data BLOCK(4K) : {
        build/obj/arch/x86/*.o (.data)
    }

    .hhk_init_rodata BLOCK(4K) : {
        build/obj/arch/x86/*.o (.rodata)
    }
    __init_hhk_end = .;

    /* Relocation of our higher half kernel */
    . += 0xC0000000;

    /* 好了，我们的内核…… */
    .text BLOCK(4K) : AT ( ADDR(.text) - 0xC0000000 ) {
        __kernel_start = .;
        build/obj/kernel/*.o (.text)
    }

    .bss BLOCK(4K) : AT ( ADDR(.bss) - 0xC0000000 ) {
        build/obj/kernel/*.o (.bss)
    }

    .data BLOCK(4k) : AT ( ADDR(.data) - 0xC0000000 ) {
        build/obj/kernel/*.o (.data)
    }

    .rodata BLOCK(4K) : AT ( ADDR(.rodata) - 0xC0000000 ) {
        build/obj/kernel/*.o (.rodata)
    }

    .kpg BLOCK(4K) : AT ( ADDR(.kpg) - 0xC0000000 ) {
        build/obj/arch/x86/*.o (.kpg)
    }

    __kernel_end = ALIGN(.);
    __heap_start = ALIGN(.);    /* 内核结束的地方即堆开始的地方 */
}