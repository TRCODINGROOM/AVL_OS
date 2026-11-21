#include <avlos/scheduler.h>
#include <avlos/arch_x86_64.h>
#include <avlos/interrupts.h>
#include <avlos/smp.h>

#define MAX_CPUS 64u
#define MAX_TASKS 96u
#define TASK_STACK_SIZE 16384u
#define NO_AFFINITY UINT32_MAX

enum task_state {
    TASK_UNUSED = 0,
    TASK_RUNNABLE,
    TASK_RUNNING,
    TASK_DEAD,
    TASK_IDLE,
};

struct task {
    uint64_t saved_stack;
    uint64_t kernel_stack_top;
    task_entry_t entry;
    void *argument;
    uint32_t affinity;
    uint32_t id;
    enum task_state state;
};

static struct task tasks[MAX_TASKS];
static struct task idle_tasks[MAX_CPUS];
static uint8_t task_stacks[MAX_TASKS][TASK_STACK_SIZE * 2u] __attribute__((aligned(16)));
static uint8_t idle_stacks[MAX_CPUS][TASK_STACK_SIZE * 2u] __attribute__((aligned(16)));
static struct task *current_tasks[MAX_CPUS];
static uint64_t bootstrap_stacks[MAX_CPUS];
static uint8_t scheduler_lock;
static volatile uint64_t cpu_ticks[MAX_CPUS];

static uint64_t irq_save(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void irq_restore(uint64_t flags) {
    if ((flags & (1ull << 9)) != 0) {
        __asm__ volatile("sti" : : : "memory");
    }
}

static void lock_scheduler(void) {
    while (__atomic_test_and_set(&scheduler_lock, __ATOMIC_ACQUIRE)) {
        __asm__ volatile("pause");
    }
}

static void unlock_scheduler(void) {
    __atomic_clear(&scheduler_lock, __ATOMIC_RELEASE);
}

static void task_bootstrap(void);
static void idle_task_entry(void *argument);

static void initialize_context(struct task *task, uint8_t *stack,
                               uint64_t stack_size,
                               uint8_t *interrupt_stack) {
    uint64_t *kernel_top = (uint64_t *)(uintptr_t)
        (((uintptr_t)(stack + stack_size)) & ~(uintptr_t)0x0full);
    uint64_t *entry_top = (uint64_t *)((uintptr_t)kernel_top - 8u);
    uint64_t *sp = entry_top;
    *--sp = (uint64_t)(uintptr_t)task_bootstrap;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    task->saved_stack = (uint64_t)(uintptr_t)sp;
    task->kernel_stack_top = (uint64_t)(uintptr_t)(interrupt_stack + stack_size);
}

int scheduler_init(void) {
    scheduler_lock = 0;
    for (uint32_t index = 0; index < MAX_CPUS; ++index) {
        current_tasks[index] = 0;
        bootstrap_stacks[index] = 0;
        idle_tasks[index].entry = idle_task_entry;
        idle_tasks[index].argument = (void *)(uintptr_t)index;
        idle_tasks[index].affinity = index;
        idle_tasks[index].id = UINT32_MAX - index;
        idle_tasks[index].state = TASK_IDLE;
        initialize_context(&idle_tasks[index], idle_stacks[index],
                           TASK_STACK_SIZE, idle_stacks[index] + TASK_STACK_SIZE);
    }
    return 0;
}

int scheduler_create(task_entry_t entry, void *argument, uint32_t cpu_affinity) {
    if (entry == 0 ||
        (cpu_affinity != NO_AFFINITY && cpu_affinity >= smp_cpu_count())) {
        return -1;
    }

    uint64_t flags = irq_save();
    lock_scheduler();
    uint32_t slot = MAX_TASKS;
    for (uint32_t index = 0; index < MAX_TASKS; ++index) {
        if (tasks[index].state == TASK_UNUSED ||
            tasks[index].state == TASK_DEAD) {
            slot = index;
            break;
        }
    }
    if (slot == MAX_TASKS) {
        unlock_scheduler();
        irq_restore(flags);
        return -1;
    }

    struct task *task = &tasks[slot];
    task->entry = entry;
    task->argument = argument;
    task->affinity = cpu_affinity;
    task->id = slot;
    task->state = TASK_RUNNABLE;
    initialize_context(task, task_stacks[slot], TASK_STACK_SIZE,
                       task_stacks[slot] + TASK_STACK_SIZE);
    unlock_scheduler();
    irq_restore(flags);
    return (int)slot;
}

static struct task *pick_next_locked(uint32_t cpu_index) {
    uint32_t start = current_tasks[cpu_index] == 0
                         ? 0
                         : (current_tasks[cpu_index]->id + 1u) % MAX_TASKS;
    for (uint32_t offset = 0; offset < MAX_TASKS; ++offset) {
        uint32_t index = (start + offset) % MAX_TASKS;
        struct task *candidate = &tasks[index];
        if (candidate->state == TASK_RUNNABLE &&
            (candidate->affinity == NO_AFFINITY ||
             candidate->affinity == cpu_index)) {
            candidate->state = TASK_RUNNING;
            return candidate;
        }
    }
    return &idle_tasks[cpu_index];
}

void scheduler_yield(void) {
    uint32_t cpu_index = smp_current_cpu();
    if (cpu_index >= MAX_CPUS) {
        cpu_index = 0;
    }
    uint64_t flags = irq_save();
    lock_scheduler();
    struct task *previous = current_tasks[cpu_index];
    if (previous != 0 && previous->state == TASK_RUNNING) {
        previous->state = TASK_RUNNABLE;
    }
    struct task *next = pick_next_locked(cpu_index);
    if (next == previous) {
        if (previous != 0 && previous->state != TASK_IDLE) {
            previous->state = TASK_RUNNING;
        }
        unlock_scheduler();
        irq_restore(flags);
        return;
    }
    current_tasks[cpu_index] = next;
    if (next->state != TASK_IDLE) {
        next->state = TASK_RUNNING;
    }
    arch_set_kernel_stack(next->kernel_stack_top);
    uint64_t *old_stack = previous != 0
                              ? &previous->saved_stack
                              : &bootstrap_stacks[cpu_index];
    uint64_t new_stack = next->saved_stack;
    unlock_scheduler();
    arch_context_switch(old_stack, new_stack);
    irq_restore(flags);
}

static void task_bootstrap(void) {
    interrupts_enable();
    uint32_t cpu_index = smp_current_cpu();
    struct task *task = current_tasks[cpu_index];
    if (task != 0 && task->entry != 0) {
        task->entry(task->argument);
    }
    scheduler_exit_current();
}

static void idle_task_entry(void *argument) {
    (void)argument;
    for (;;) {
        if (smp_x2apic_available() || smp_is_bsp()) {
            __asm__ volatile("sti; hlt; cli" : : : "memory");
        } else {
            __asm__ volatile("pause");
        }
        scheduler_yield();
    }
}

void scheduler_exit_current(void) {
    uint32_t cpu_index = smp_current_cpu();
    uint64_t flags = irq_save();
    lock_scheduler();
    struct task *current = current_tasks[cpu_index];
    if (current != 0 && current->state != TASK_IDLE) {
        current->state = TASK_DEAD;
    }
    unlock_scheduler();
    irq_restore(flags);
    for (;;) {
        scheduler_yield();
    }
}

void scheduler_timer_tick(void) {
    uint32_t cpu_index = smp_current_cpu();
    if (cpu_index < MAX_CPUS) {
        __atomic_add_fetch(&cpu_ticks[cpu_index], 1, __ATOMIC_RELAXED);
        if (current_tasks[cpu_index] != 0) {
            scheduler_yield();
        }
    }
}

void scheduler_start(void) {
    scheduler_cpu_start(smp_current_cpu());
}

void scheduler_cpu_start(uint32_t cpu_index) {
    smp_set_cpu(cpu_index);
    for (;;) {
        scheduler_yield();
    }
}

uint64_t scheduler_task_count(void) {
    uint64_t flags = irq_save();
    lock_scheduler();
    uint64_t active = 0;
    for (uint32_t index = 0; index < MAX_TASKS; ++index) {
        if (tasks[index].state == TASK_RUNNABLE ||
            tasks[index].state == TASK_RUNNING) {
            ++active;
        }
    }
    unlock_scheduler();
    irq_restore(flags);
    return active;
}

uint32_t scheduler_current_cpu(void) {
    return smp_current_cpu();
}

