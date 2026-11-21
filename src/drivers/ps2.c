#include <avlos/arch_x86_64.h>
#include <avlos/input.h>

#define INPUT_RING_SIZE 128u

static struct input_event input_ring[INPUT_RING_SIZE];
static volatile uint32_t input_head;
static volatile uint32_t input_tail;
static uint8_t mouse_packet[3];
static uint8_t mouse_packet_index;
static uint8_t keyboard_extended;

static void input_push(const struct input_event *event) {
    uint32_t head = __atomic_load_n(&input_head, __ATOMIC_RELAXED);
    uint32_t next = (head + 1u) % INPUT_RING_SIZE;
    if (next == __atomic_load_n(&input_tail, __ATOMIC_ACQUIRE)) {
        return;
    }
    input_ring[head] = *event;
    __atomic_store_n(&input_head, next, __ATOMIC_RELEASE);
}

static int wait_input_clear(void) {
    for (uint32_t spin = 0; spin < 100000u; ++spin) {
        if ((x86_inb(0x64) & 0x02u) == 0) {
            return 1;
        }
        __asm__ volatile("pause");
    }
    return 0;
}

static int wait_output_full(void) {
    for (uint32_t spin = 0; spin < 100000u; ++spin) {
        if ((x86_inb(0x64) & 0x01u) != 0) {
            return 1;
        }
        __asm__ volatile("pause");
    }
    return 0;
}

static int controller_command(uint8_t command) {
    if (!wait_input_clear()) {
        return 0;
    }
    x86_outb(0x64, command);
    return 1;
}

static int controller_data(uint8_t value) {
    if (!wait_input_clear()) {
        return 0;
    }
    x86_outb(0x60, value);
    return 1;
}

static int mouse_command(uint8_t value) {
    if (!controller_command(0xd4) || !controller_data(value) ||
        !wait_output_full()) {
        return 0;
    }
    return x86_inb(0x60) == 0xfau;
}

static void keyboard_command(uint8_t value) {
    if (!controller_data(value) || !wait_output_full()) {
        return;
    }
    uint8_t response = x86_inb(0x60);
    if (response == 0xfeu && controller_data(value) && wait_output_full()) {
        (void)x86_inb(0x60);
    }
}

void input_init(void) {
    input_head = 0;
    input_tail = 0;
    mouse_packet_index = 0;
    keyboard_extended = 0;

    controller_command(0xad);
    controller_command(0xa7);
    (void)x86_inb(0x60);
    controller_command(0x20);
    uint8_t config = wait_output_full() ? x86_inb(0x60) : 0;
    config |= 0x03u;
    config &= (uint8_t)~0x30u;
    controller_command(0x60);
    controller_data(config);
    controller_command(0xae);
    controller_command(0xa8);
    keyboard_command(0xf4);
    (void)mouse_command(0xf4);
}

void input_keyboard_irq(void) {
    uint8_t scancode = x86_inb(0x60);
    if (scancode == 0xe0u || scancode == 0xe1u) {
        keyboard_extended = 1;
        return;
    }
    struct input_event event = {0};
    event.type = INPUT_EVENT_KEY;
    event.pressed = (scancode & 0x80u) == 0;
    event.code = scancode & 0x7fu;
    if (keyboard_extended) {
        event.code |= 0x80u;
        keyboard_extended = 0;
    }
    input_push(&event);
}

void input_mouse_irq(void) {
    uint8_t byte = x86_inb(0x60);
    if (mouse_packet_index == 0 && (byte & 0x08u) == 0) {
        return;
    }
    mouse_packet[mouse_packet_index++] = byte;
    if (mouse_packet_index < 3) {
        return;
    }
    mouse_packet_index = 0;
    struct input_event event = {0};
    event.type = INPUT_EVENT_MOUSE;
    event.buttons = mouse_packet[0] & 0x07u;
    event.dx = (int8_t)mouse_packet[1];
    event.dy = -(int8_t)mouse_packet[2];
    input_push(&event);
}

int input_pop(struct input_event *event) {
    if (event == 0) {
        return 0;
    }
    uint32_t tail = __atomic_load_n(&input_tail, __ATOMIC_RELAXED);
    if (tail == __atomic_load_n(&input_head, __ATOMIC_ACQUIRE)) {
        return 0;
    }
    *event = input_ring[tail];
    __atomic_store_n(&input_tail, (tail + 1u) % INPUT_RING_SIZE,
                     __ATOMIC_RELEASE);
    return 1;
}

