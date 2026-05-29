.section .text
.align 3
.globl _start

_start:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov x19, x0
    mov x20, x1
    bl efi_main
    mov w0, #0
    ldp x29, x30, [sp], #16
    ret
