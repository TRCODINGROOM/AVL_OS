#ifndef AVLOS_ARCH_X86_64_H
#define AVLOS_ARCH_X86_64_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void avlos_arch_halt(void) __attribute__((noreturn));
void x86_outb(uint16_t port, uint8_t value);
uint8_t x86_inb(uint16_t port);
void x86_outw(uint16_t port, uint16_t value);
uint16_t x86_inw(uint16_t port);
void x86_outl(uint16_t port, uint32_t value);
uint32_t x86_inl(uint16_t port);
void x86_insw(uint16_t port, void *buffer, uint64_t words);
void x86_outsw(uint16_t port, const void *buffer, uint64_t words);
uint64_t arch_rdmsr(uint32_t msr);
void arch_wrmsr(uint32_t msr, uint64_t value);
void arch_load_gdt(const void *descriptor);
void arch_load_tr(uint16_t selector);
void arch_context_switch(uint64_t *old_stack, uint64_t new_stack);

#ifdef __cplusplus
}
#endif

#endif

