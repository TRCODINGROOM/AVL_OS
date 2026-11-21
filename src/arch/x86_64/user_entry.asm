bits 64
default rel

section .text
global arch_enter_user

arch_enter_user:
    cli
    mov ax, 0x1b
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    swapgs
    push qword 0x1b
    push rsi
    pushfq
    pop rax
    or rax, 0x200
    push rax
    push qword 0x23
    push rdi
    iretq

