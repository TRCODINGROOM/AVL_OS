#ifndef AVLOS_SCHEDULER_H
#define AVLOS_SCHEDULER_H

#include <stdint.h>

typedef void (*task_entry_t)(void *);

int scheduler_init(void);
int scheduler_create(task_entry_t entry, void *argument, uint32_t cpu_affinity);
void scheduler_start(void) __attribute__((noreturn));
void scheduler_cpu_start(uint32_t cpu_index) __attribute__((noreturn));
void scheduler_yield(void);
void scheduler_exit_current(void) __attribute__((noreturn));
void scheduler_timer_tick(void);
uint64_t scheduler_task_count(void);
uint32_t scheduler_current_cpu(void);

#endif

