#include <avlos/audio.h>
#include <avlos/arch_x86_64.h>
#include <avlos/interrupts.h>

static int speaker_ready;

void audio_init(void) {
    speaker_ready = 1;
}

void audio_beep(uint32_t frequency_hz, uint32_t duration_ms) {
    if (!speaker_ready || frequency_hz < 20u || frequency_hz > 20000u ||
        duration_ms == 0) {
        return;
    }
    uint32_t divisor = 1193182u / frequency_hz;
    if (divisor == 0 || divisor > 65535u) {
        return;
    }
    x86_outb(0x43, 0xb6u);
    x86_outb(0x42, (uint8_t)divisor);
    x86_outb(0x42, (uint8_t)(divisor >> 8));
    uint8_t control = x86_inb(0x61);
    if ((control & 0x03u) != 0x03u) {
        x86_outb(0x61, control | 0x03u);
    }

    uint64_t until = interrupt_ticks() + (duration_ms / 10u) + 1u;
    while (interrupt_ticks() < until) {
        __asm__ volatile("pause");
    }
    audio_stop();
}

void audio_stop(void) {
    uint8_t control = x86_inb(0x61);
    x86_outb(0x61, control & (uint8_t)~0x03u);
}

