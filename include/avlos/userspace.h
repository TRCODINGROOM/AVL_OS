#ifndef AVLOS_USERSPACE_H
#define AVLOS_USERSPACE_H

#include <avlos/interrupts.h>
#include <stdint.h>

int userspace_prepare_demo(void);
int userspace_available(void);
void userspace_enter_demo(void) __attribute__((noreturn));
uint64_t syscall_dispatch(struct interrupt_frame *frame);

#endif

