ENTRY(_start)

SECTIONS {
    . = 0x400000;

    .text BLOCK(4K) : {
        *(.text)
    }

    .data BLOCK(4K) : {
        *(.data)
    }

    .ro.data BLOCK(4K) : {
        *(.ro.data)
    }

    .bss BLOCK(4K) : {
        *(.bss)
    }
}
