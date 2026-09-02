OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(kernel_entry)

PHDRS
{
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x10000;
    __kernel_start = .;

    .text ALIGN(16) :
    {
        *(.text .text.*)
    } :text

    .rodata ALIGN(0x1000) :
    {
        *(.rodata .rodata.*)
    } :rodata

    .data ALIGN(0x1000) :
    {
        *(.data .data.*)
    } :data

    .kaon_globals ALIGN(16) (NOLOAD) :
    {
        __bss_start = .;
        __kaon_globals_start = .;
        *(.kaon_globals .kaon_globals.*)
        __kaon_globals_end = .;
    } :data

    .bss ALIGN(16) (NOLOAD) :
    {
        *(COMMON)
        *(.bss .bss.*)
        __bss_end = .;
    } :data

    __kernel_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame .eh_frame.*)
        *(.note .note.*)
    }
}
