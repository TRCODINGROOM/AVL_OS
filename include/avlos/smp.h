#ifndef AVLOS_SMP_H
#define AVLOS_SMP_H

#include <stdint.h>
#include <limine.h>

int smp_prepare(struct limine_mp_response *response);
uint32_t smp_start_aps(void);
uint32_t smp_cpu_count(void);
uint32_t smp_online_cpu_count(void);
uint32_t smp_current_cpu(void);
uint32_t smp_bsp_cpu(void);
int smp_is_bsp(void);
void smp_set_cpu(uint32_t cpu_index);
void smp_ap_entry(struct limine_mp_info *cpu) __attribute__((noreturn));
int smp_x2apic_available(void);
void smp_local_timer_init(void);
void smp_send_eoi(void);

#endif

