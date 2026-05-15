_bin_base = 0x400;
_bin_sz = 4K;

STARTUP(crt0.o)
OUTPUT_ARCH(m68k)

__code_start = _bin_base;
__code_sz = _bin_sz;

MEMORY {
    code (rwx) : ORIGIN = __code_start, LENGTH = __code_sz
}

SECTIONS {
    .text : {
        _text_start = .;
        *(.text .text.*)
        . = ALIGN(2);
        _text_end = .;
    } > code

    .data : AT(_text_end) {
        _data_start = .;
        *(.rodata .rodata.*)
        *(.data .data.*)
        . = ALIGN(2);
        _data_end = .;
    } > code

    .bss : AT(_data_end) {
        _bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(2);
        _bss_end = .;
    } > code
}
