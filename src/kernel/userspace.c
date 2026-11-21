#include <avlos/userspace.h>
#include <avlos/memory.h>
#include <avlos/paging.h>
#include <avlos/scheduler.h>
#include <avlos/arch_x86_64.h>
#include <avlos/kernel.h>

#define USER_CODE_ADDRESS 0x00400000ull
#define USER_STACK_ADDRESS 0x007ff000ull
#define PAGE_SIZE 4096ull

static int demo_ready;

int userspace_prepare_demo(void) {
    if (demo_ready) {
        return 1;
    }

    uint64_t code_physical = pmm_alloc_frame();
    uint64_t stack_physical = pmm_alloc_frame();
    if (code_physical == 0 || stack_physical == 0) {
        if (code_physical != 0) pmm_free_frame(code_physical);
        if (stack_physical != 0) pmm_free_frame(stack_physical);
        return 0;
    }

    static const uint8_t program[] = {
        0xb8, 0x01, 0x00, 0x00, 0x00,       /* write */
        0xbf, 0x41, 0x00, 0x00, 0x00,       /* 'A' */
        0xcd, 0x80,
        0xb8, 0x02, 0x00, 0x00, 0x00,       /* yield */
        0xcd, 0x80,
        0xb8, 0x03, 0x00, 0x00, 0x00,       /* exit */
        0xcd, 0x80,
        0x0f, 0x0b,
    };
    uint8_t *code = (uint8_t *)phys_to_virt(code_physical);
    uint8_t *stack = (uint8_t *)phys_to_virt(stack_physical);
    for (uint32_t index = 0; index < sizeof(program); ++index) {
        code[index] = program[index];
    }
    for (uint32_t index = 0; index < PAGE_SIZE; ++index) {
        stack[index] = 0;
    }

    uint64_t stack_flags = PAGE_USER | PAGE_WRITE;
    if (paging_nx_available()) stack_flags |= PAGE_NX;
    if (page_map(USER_CODE_ADDRESS, code_physical, PAGE_USER) != 0 ||
        page_map(USER_STACK_ADDRESS, stack_physical, stack_flags) != 0) {
        page_unmap(USER_CODE_ADDRESS);
        pmm_free_frame(code_physical);
        pmm_free_frame(stack_physical);
        return 0;
    }
    demo_ready = 1;
    return 1;
}

int userspace_available(void) {
    return demo_ready;
}

void userspace_enter_demo(void) {
    if (!demo_ready) {
        scheduler_exit_current();
    }
    arch_enter_user(USER_CODE_ADDRESS,
                    USER_STACK_ADDRESS + PAGE_SIZE - 16u);
    scheduler_exit_current();
}

uint64_t syscall_dispatch(struct interrupt_frame *frame) {
    if (frame == 0) {
        return (uint64_t)-1;
    }
    switch (frame->rax) {
        case 1: {
            char output[2];
            output[0] = (char)frame->rdi;
            output[1] = '\0';
            avlos_serial_write(output);
            return 1;
        }
        case 2:
            scheduler_yield();
            return 0;
        case 3:
            scheduler_exit_current();
        default:
            return (uint64_t)-1;
    }
}

