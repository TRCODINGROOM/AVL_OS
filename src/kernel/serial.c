#include <avlos/kernel.h>
#include <avlos/arch_x86_64.h>

#define COM1 0x3f8u

void avlos_serial_initialize(void) {
    x86_outb(COM1 + 1u, 0x00u);
    x86_outb(COM1 + 3u, 0x80u);
    x86_outb(COM1 + 0u, 0x01u);
    x86_outb(COM1 + 1u, 0x00u);
    x86_outb(COM1 + 3u, 0x03u);
    x86_outb(COM1 + 2u, 0xc7u);
    x86_outb(COM1 + 4u, 0x0bu);
}

static void serial_write_character(char character) {
    for (uint32_t spins = 0; spins < 100000u; ++spins) {
        if ((x86_inb(COM1 + 5u) & 0x20u) != 0) {
            x86_outb(COM1, (uint8_t)character);
            return;
        }
    }
}

void avlos_serial_write(const char *text) {
    if (text == 0) {
        return;
    }
    while (*text != '\0') {
        serial_write_character(*text++);
    }
}

