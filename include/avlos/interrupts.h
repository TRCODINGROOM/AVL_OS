#ifndef AVLOS_INTERRUPTS_H
#define AVLOS_INTERRUPTS_H

#include <stdint.h>

struct interrupt_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

void interrupts_init(void);
void interrupts_ap_init(void);
void interrupts_enable(void);
void interrupts_disable(void);
uint64_t interrupt_ticks(void);
void interrupt_dispatch(struct interrupt_frame *frame);
void arch_set_kernel_stack(uint64_t stack_top);
void arch_enter_user(uint64_t instruction_pointer, uint64_t stack_pointer);

#endif

