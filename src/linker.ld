OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);   /* PF_R | PF_X */
    rodata PT_LOAD FLAGS(4); /* PF_R */
    data PT_LOAD FLAGS(6);   /* PF_R | PF_W */
}

SECTIONS
{
    . = 0;
    .text : {
        *(.text*)
        *(.got*)
        . = ALIGN(16);
    } :text
    .rodata : {
        *(.rodata*)
        . = ALIGN(16);
    } :rodata
    .data : {
        *(.data*)
        . = ALIGN(16);
    } :data
    .bss : {
        *(.bss*)
        . = ALIGN(16);
    } :data
    .rela.dyn : { *(.rela.dyn) }
    .rela.plt : { *(.rela.plt) }
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.ARM.attributes)
        *(.debug*)
    }
    . = ALIGN(4096);
    _end = .;
}
