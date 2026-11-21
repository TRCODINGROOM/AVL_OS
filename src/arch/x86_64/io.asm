bits 64
default rel

section .text
global avlos_arch_halt
global x86_outb
global x86_inb
global x86_outw
global x86_inw
global x86_outl
global x86_inl
global x86_insw
global x86_outsw
global arch_rdmsr
global arch_wrmsr
global arch_load_gdt
global arch_load_tr
global arch_context_switch

avlos_arch_halt:
    cli
.halt_loop:
    hlt
    jmp .halt_loop

x86_outb:
    mov dx, di
    mov al, sil
    out dx, al
    ret

x86_inb:
    mov dx, di
    xor eax, eax
    in al, dx
    ret

x86_outw:
    mov dx, di
    mov ax, si
    out dx, ax
    ret

x86_inw:
    mov dx, di
    xor eax, eax
    in ax, dx
    ret

x86_outl:
    mov dx, di
    mov eax, esi
    out dx, eax
    ret

x86_inl:
    mov dx, di
    xor eax, eax
    in eax, dx
    ret

x86_insw:
    mov rcx, rdx
    mov dx, di
    mov rdi, rsi
    cld
    rep insw
    ret

x86_outsw:
    mov rcx, rdx
    mov dx, di
    cld
    rep outsw
    ret

arch_rdmsr:
    mov ecx, edi
    rdmsr
    shl rdx, 32
    or rax, rdx
    ret

arch_wrmsr:
    mov ecx, edi
    mov eax, esi
    mov rdx, rsi
    shr rdx, 32
    wrmsr
    ret

arch_load_gdt:
    lgdt [rdi]
    push qword 0x08
    lea rax, [rel .gdt_reloaded]
    push rax
    retfq
.gdt_reloaded:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

arch_load_tr:
    mov ax, di
    ltr ax
    ret

arch_context_switch:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov [rdi], rsp
    mov rsp, rsi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

