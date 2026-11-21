bits 64
default rel

section .text.boot
global _start
extern kernel_main
extern avlos_arch_halt

_start:
    cli
    cld
    lea rsp, [rel boot_stack_top]
    and rsp, -16
    call kernel_main
    call avlos_arch_halt

section .bss nobits align=16
boot_stack:
    resb 65536
boot_stack_top:

