#include <avlos/interrupts.h>
#include <avlos/arch_x86_64.h>
#include <avlos/kernel.h>
#include <avlos/input.h>
#include <avlos/scheduler.h>
#include <avlos/smp.h>
#include <avlos/userspace.h>

#define IDT_COUNT 256u
#define KERNEL_CODE_SELECTOR 0x08u
#define KERNEL_DATA_SELECTOR 0x10u
#define USER_DATA_SELECTOR 0x1bu
#define USER_CODE_SELECTOR 0x23u
#define TSS_SELECTOR 0x28u
#define MAX_CPUS 64u

struct idt_gate {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attributes;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct descriptor_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct tss64 {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

static struct idt_gate idt[IDT_COUNT] __attribute__((aligned(16)));
struct cpu_tables {
    uint64_t gdt[7] __attribute__((aligned(16)));
    struct tss64 tss;
    struct descriptor_pointer gdtr;
};

static struct cpu_tables cpu_tables[MAX_CPUS];
static struct descriptor_pointer idtr;
static volatile uint64_t ticks;

extern void *isr_stub_table[48];
extern void isr128(void);
extern void isr240(void);
extern void isr241(void);
extern void isr255(void);

static void idt_set_gate(uint8_t vector, void *handler, uint8_t attributes) {
    uint64_t address = (uint64_t)(uintptr_t)handler;
    idt[vector].offset_low = (uint16_t)address;
    idt[vector].selector = KERNEL_CODE_SELECTOR;
    idt[vector].ist = 0;
    idt[vector].type_attributes = attributes;
    idt[vector].offset_middle = (uint16_t)(address >> 16);
    idt[vector].offset_high = (uint32_t)(address >> 32);
    idt[vector].reserved = 0;
}

static void gdt_set_tss(uint64_t *gdt, uint64_t base, uint32_t limit) {
    uint64_t low = 0;
    low |= limit & 0xffffu;
    low |= (base & 0xffffffu) << 16;
    low |= 0x89ull << 40;
    low |= ((uint64_t)(limit >> 16) & 0x0fu) << 48;
    low |= ((base >> 24) & 0xffu) << 56;
    gdt[5] = low;
    gdt[6] = base >> 32;
}

static void load_cpu_tables(uint32_t cpu_index) {
    if (cpu_index >= MAX_CPUS) cpu_index = 0;
    struct cpu_tables *tables = &cpu_tables[cpu_index];
    tables->gdt[0] = 0;
    tables->gdt[1] = 0x00af9a000000ffffull;
    tables->gdt[2] = 0x00cf92000000ffffull;
    tables->gdt[3] = 0x00cff2000000ffffull;
    tables->gdt[4] = 0x00affa000000ffffull;
    tables->tss = (struct tss64){0};
    tables->tss.iomap_base = sizeof(tables->tss);
    gdt_set_tss(tables->gdt, (uint64_t)(uintptr_t)&tables->tss,
                sizeof(tables->tss) - 1u);
    tables->gdtr.limit = sizeof(tables->gdt) - 1u;
    tables->gdtr.base = (uint64_t)(uintptr_t)tables->gdt;
    arch_load_gdt(&tables->gdtr);
    arch_load_tr(TSS_SELECTOR);
}

static void pic_remap(void) {
    uint8_t master_mask = x86_inb(0x21);
    uint8_t slave_mask = x86_inb(0xa1);
    x86_outb(0x20, 0x11);
    x86_outb(0xa0, 0x11);
    x86_outb(0x21, 0x20);
    x86_outb(0xa1, 0x28);
    x86_outb(0x21, 0x04);
    x86_outb(0xa1, 0x02);
    x86_outb(0x21, 0x01);
    x86_outb(0xa1, 0x01);
    (void)master_mask;
    (void)slave_mask;
    x86_outb(0x21, 0xf8);
    x86_outb(0xa1, 0xef);
}

static void pit_initialize(uint32_t frequency_hz) {
    uint32_t divisor = 1193182u / frequency_hz;
    x86_outb(0x43, 0x36);
    x86_outb(0x40, (uint8_t)divisor);
    x86_outb(0x40, (uint8_t)(divisor >> 8));
}

void interrupts_init(void) {
    for (uint32_t index = 0; index < IDT_COUNT; ++index) {
        idt_set_gate((uint8_t)index, isr255, 0x8e);
    }
    for (uint32_t index = 0; index < 48; ++index) {
        idt_set_gate((uint8_t)index, isr_stub_table[index], 0x8e);
    }
    idt_set_gate(128, isr128, 0xee);
    idt_set_gate(240, isr240, 0x8e);
    idt_set_gate(241, isr241, 0x8e);

    idtr.limit = sizeof(idt) - 1u;
    idtr.base = (uint64_t)(uintptr_t)idt;

    load_cpu_tables(smp_current_cpu());
    __asm__ volatile("lidt %0" : : "m"(idtr));

    pic_remap();
    pit_initialize(100);
}

void interrupts_ap_init(void) {
    load_cpu_tables(smp_current_cpu());
    __asm__ volatile("lidt %0" : : "m"(idtr));
}

void interrupts_enable(void) {
    __asm__ volatile("sti" : : : "memory");
}

void interrupts_disable(void) {
    __asm__ volatile("cli" : : : "memory");
}

uint64_t interrupt_ticks(void) {
    return __atomic_load_n(&ticks, __ATOMIC_RELAXED);
}

void arch_set_kernel_stack(uint64_t stack_top) {
    uint32_t cpu_index = smp_current_cpu();
    if (cpu_index >= MAX_CPUS) cpu_index = 0;
    cpu_tables[cpu_index].tss.rsp0 = stack_top;
}

static void interrupt_fatal(uint64_t vector, uint64_t error_code) {
    avlos_serial_write("AVLOS: CPU exception vector=");
    (void)vector;
    (void)error_code;
    interrupts_disable();
    avlos_arch_halt();
}

void interrupt_dispatch(struct interrupt_frame *frame) {
    uint64_t vector = frame->vector;
    if (vector < 32) {
        interrupt_fatal(vector, frame->error_code);
    }
    if (vector == 32) {
        __atomic_add_fetch(&ticks, 1, __ATOMIC_RELAXED);
        x86_outb(0x20, 0x20);
        scheduler_timer_tick();
        return;
    }
    if (vector == 33) {
        input_keyboard_irq();
        x86_outb(0x20, 0x20);
        return;
    }
    if (vector == 44) {
        input_mouse_irq();
        x86_outb(0xa0, 0x20);
        x86_outb(0x20, 0x20);
        return;
    }
    if (vector == 128) {
        frame->rax = syscall_dispatch(frame);
        return;
    }
    if (vector == 240) {
        smp_send_eoi();
        scheduler_timer_tick();
        return;
    }
    if (vector == 241) {
        smp_send_eoi();
        return;
    }
    if (vector >= 40 && vector <= 47) {
        x86_outb(0xa0, 0x20);
        x86_outb(0x20, 0x20);
        return;
    }
    if (vector >= 32 && vector <= 39) {
        x86_outb(0x20, 0x20);
    }
}

