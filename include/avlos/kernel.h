#ifndef AVLOS_KERNEL_H
#define AVLOS_KERNEL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct avlos_framebuffer {
    volatile uint8_t *address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
};

struct avlos_boot_info {
    struct avlos_framebuffer framebuffer;
    uint64_t memory_map_entries;
    uint64_t firmware_type;
    uint64_t rust_probe;
    uint8_t rust_abi_ok;
};

struct avlos_runtime_status {
    uint64_t cpu_count;
    uint64_t free_frames;
    uint64_t task_count;
    uint64_t cpu_work_total;
    uint64_t keyboard_events;
    uint64_t mouse_events;
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint8_t storage_ready;
    uint8_t network_ready;
    uint8_t audio_ready;
    uint8_t acpi_ready;
    uint8_t userspace_ready;
};

void kernel_main(void);

void avlos_serial_initialize(void);
void avlos_serial_write(const char *text);
uint64_t kernel_cpu_work_total(void);

void avlos_fb_clear(const struct avlos_framebuffer *framebuffer, uint32_t rgb);
void avlos_fb_pixel(const struct avlos_framebuffer *framebuffer,
                   uint32_t x, uint32_t y, uint32_t rgb);
void avlos_fb_fill_rect(const struct avlos_framebuffer *framebuffer,
                       uint32_t x, uint32_t y,
                       uint32_t width, uint32_t height, uint32_t rgb);
void avlos_ui_draw_boot_screen(const struct avlos_boot_info *boot_info);
void avlos_ui_draw_runtime_overlay(const struct avlos_boot_info *boot_info,
                                  const struct avlos_runtime_status *status);

uint64_t avlos_rust_fnv1a64(const uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif

