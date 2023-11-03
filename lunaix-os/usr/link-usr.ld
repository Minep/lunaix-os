ENTRY(_start)

SECTIONS {
    . = 0x400000;

    .text : {
        *(.text)
    }

    .data : {
        *(.data)
    }

    .ro.data : {
        *(.ro.data)
    }

    .bss : {
        *(.bss)
    }
}
