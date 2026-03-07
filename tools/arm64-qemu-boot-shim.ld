/* Linker script for arm64 QEMU boot shim. */
ENTRY(_start)
SECTIONS {
    . = 0x40200000;
    .text.header : { *(.text.header) }
    .text : { *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .data : { *(.data .data.*) }
    .bss : { *(.bss .bss.*) }
    . = ALIGN(64);
    _image_end = .;
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
