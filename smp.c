#include <avlos/smp.h>
#include <avlos/arch_x86_64.h>
#include <avlos/interrupts.h>
#include <avlos/scheduler.h>

#define MAX_CPUS 64u
#define MSR_IA32_APIC_BASE 0x1b
#define MSR_IA32_GS_BASE 0xc0000101u
#define MSR_IA32_KERNEL_GS_BASE 0xc0000102u
#define X2APIC_EOI 0x80b
#define X2APIC_TPR 0x808
#define X2APIC_SVR 0x80f
#define X2APIC_LVT_TIMER 0x832
#define X2APIC_INITIAL_COUNT 0x838
#define X2APIC_DIVIDE_CONFIG 0x83e

struct cpu_local {
    uint32_t cpu_index;
    uint32_t lapic_id;
    uint64_t timer_ticks;
};

static struct cpu_local cpu_locals[MAX_CPUS] __attribute__((aligned(64)));
static struct limine_mp_response *mp_response;
static uint32_t cpu_total = 1;
static uint32_t bsp_index;
static volatile uint32_t online_cpus = 1;
static int x2apic_enabled;

void smp_set_cpu(uint32_t cpu_index) {
    if (cpu_index >= MAX_CPUS) {
        cpu_index = 0;
    }
    arch_wrmsr(MSR_IA32_GS_BASE,
               (uint64_t)(uintptr_t)&cpu_locals[cpu_index]);
    arch_wrmsr(MSR_IA32_KERNEL_GS_BASE, 0);
}

uint32_t smp_current_cpu(void) {
    /* Identify the CPU from its APIC ID via CPUID instead of reading GS,
       because GS base can be 0 while swapgs state is switched. */
    uint32_t eax, ebx, ecx, edx;
    uint32_t apic_id;
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(0u), "c"(0u));
    if (eax >= 0xbu) {
        __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                         : "a"(0xbu), "c"(0u));
        apic_id = edx;
    } else {
        __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                         : "a"(1u), "c"(0u));
        apic_id = ebx >> 24;
    }
    for (uint32_t index = 0; index < cpu_total && index < MAX_CPUS; ++index) {
        if (cpu_locals[index].lapic_id == apic_id) {
            return index;
        }
    }
    return 0;
}

uint32_t smp_cpu_count(void) {
    return cpu_total;
}

uint32_t smp_online_cpu_count(void) {
    return __atomic_load_n(&online_cpus, __ATOMIC_ACQUIRE);
}

uint32_t smp_bsp_cpu(void) {
    return bsp_index;
}

int smp_is_bsp(void) {
    return smp_current_cpu() == bsp_index;
}

int smp_x2apic_available(void) {
    return x2apic_enabled;
}

int smp_prepare(struct limine_mp_response *response) {
    mp_response = response;
    cpu_total = 1;
    bsp_index = 0;
    x2apic_enabled = 0;
    online_cpus = 1;
    if (response != 0 && response->cpus != 0 && response->cpu_count != 0) {
        cpu_total = response->cpu_count > MAX_CPUS
                        ? MAX_CPUS
                        : (uint32_t)response->cpu_count;
        for (uint32_t index = 0; index < cpu_total; ++index) {
            struct limine_mp_info *cpu = response->cpus[index];
            if (cpu == 0) {
                continue;
            }
            cpu_locals[index].cpu_index = index;
            cpu_locals[index].lapic_id = cpu->lapic_id;
            cpu_locals[index].timer_ticks = 0;
            if (cpu->lapic_id == response->bsp_lapic_id) {
                bsp_index = index;
            }
        }
        x2apic_enabled =
            (response->flags & LIMINE_MP_RESPONSE_X86_64_X2APIC) != 0;
    } else {
        cpu_locals[0].cpu_index = 0;
        cpu_locals[0].lapic_id = 0;
        cpu_locals[0].timer_ticks = 0;
    }
    smp_set_cpu(bsp_index);
    return 0;
}

static void wait_for_aps(void) {
    uint32_t limit = 20000000u;
    while (__atomic_load_n(&online_cpus, __ATOMIC_ACQUIRE) < cpu_total &&
           limit-- != 0) {
        __asm__ volatile("pause");
    }
}

uint32_t smp_start_aps(void) {
    if (mp_response == 0 || mp_response->cpus == 0) {
        return 1;
    }

    for (uint32_t index = 0; index < cpu_total; ++index) {
        if (index == bsp_index) {
            continue;
        }
        struct limine_mp_info *cpu = mp_response->cpus[index];
        if (cpu == 0) {
            continue;
        }
        cpu->extra_argument = index;
        __atomic_store_n(&cpu->goto_address, smp_ap_entry, __ATOMIC_RELEASE);
    }
    wait_for_aps();
    return __atomic_load_n(&online_cpus, __ATOMIC_ACQUIRE);
}

void smp_ap_entry(struct limine_mp_info *cpu) {
    uint32_t index = (uint32_t)cpu->extra_argument;
    if (index >= cpu_total) {
        index = 0;
    }
    smp_set_cpu(index);
    interrupts_ap_init();
    if (x2apic_enabled) {
        smp_local_timer_init();
    }
    __atomic_add_fetch(&online_cpus, 1, __ATOMIC_RELEASE);
    scheduler_cpu_start(index);
}

void smp_local_timer_init(void) {
    if (!x2apic_enabled) {
        return;
    }
    uint64_t apic_base = arch_rdmsr(MSR_IA32_APIC_BASE);
    apic_base |= (1ull << 11) | (1ull << 10);
    arch_wrmsr(MSR_IA32_APIC_BASE, apic_base);
    arch_wrmsr(X2APIC_TPR, 0);
    arch_wrmsr(X2APIC_SVR, 0x100u | 0xffu);
    arch_wrmsr(X2APIC_DIVIDE_CONFIG, 0x3u);
    arch_wrmsr(X2APIC_LVT_TIMER, 240u | (1u << 17));
    arch_wrmsr(X2APIC_INITIAL_COUNT, 10000000u);
}

void smp_send_eoi(void) {
    if (x2apic_enabled) {
        arch_wrmsr(X2APIC_EOI, 0);
    }
}

