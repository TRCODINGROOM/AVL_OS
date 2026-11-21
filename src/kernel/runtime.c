#include <stddef.h>
#include <stdint.h>
#include <avlos/kernel.h>

void *memset(void *destination, int value, size_t count) {
    uint8_t *output = (uint8_t *)destination;
    for (size_t index = 0; index < count; ++index) {
        output[index] = (uint8_t)value;
    }
    return destination;
}

void *memcpy(void *destination, const void *source, size_t count) {
    uint8_t *output = (uint8_t *)destination;
    const uint8_t *input = (const uint8_t *)source;
    for (size_t index = 0; index < count; ++index) {
        output[index] = input[index];
    }
    return destination;
}

void *memmove(void *destination, const void *source, size_t count) {
    uint8_t *output = (uint8_t *)destination;
    const uint8_t *input = (const uint8_t *)source;
    uintptr_t output_address = (uintptr_t)destination;
    uintptr_t input_address = (uintptr_t)source;
    if (output_address < input_address) {
        for (size_t index = 0; index < count; ++index) {
            output[index] = input[index];
        }
    } else if (output_address > input_address) {
        for (size_t index = count; index > 0; --index) {
            output[index - 1] = input[index - 1];
        }
    }
    return destination;
}

int memcmp(const void *left, const void *right, size_t count) {
    const uint8_t *a = (const uint8_t *)left;
    const uint8_t *b = (const uint8_t *)right;
    for (size_t index = 0; index < count; ++index) {
        if (a[index] != b[index]) {
            return (int)a[index] - (int)b[index];
        }
    }
    return 0;
}

static uint32_t scale_channel(uint8_t value, uint8_t mask_size,
                              uint8_t mask_shift) {
    if (mask_size == 0 || mask_size > 8 || mask_shift >= 32) {
        return 0;
    }
    uint32_t maximum = (1u << mask_size) - 1u;
    return ((uint32_t)value * maximum / 255u) << mask_shift;
}

static uint32_t encode_rgb(const struct avlos_framebuffer *framebuffer,
                           uint32_t rgb) {
    uint8_t red = (uint8_t)(rgb >> 16);
    uint8_t green = (uint8_t)(rgb >> 8);
    uint8_t blue = (uint8_t)rgb;
    return scale_channel(red, framebuffer->red_mask_size,
                         framebuffer->red_mask_shift) |
           scale_channel(green, framebuffer->green_mask_size,
                         framebuffer->green_mask_shift) |
           scale_channel(blue, framebuffer->blue_mask_size,
                         framebuffer->blue_mask_shift);
}

void avlos_fb_pixel(const struct avlos_framebuffer *framebuffer,
                   uint32_t x, uint32_t y, uint32_t rgb) {
    if (framebuffer == 0 || framebuffer->address == 0 ||
        x >= framebuffer->width || y >= framebuffer->height ||
        (framebuffer->bpp != 24 && framebuffer->bpp != 32)) {
        return;
    }

    uint32_t bytes_per_pixel = framebuffer->bpp / 8u;
    volatile uint8_t *pixel = framebuffer->address +
                              (uint64_t)y * framebuffer->pitch +
                              (uint64_t)x * bytes_per_pixel;
    uint32_t encoded = encode_rgb(framebuffer, rgb);
    pixel[0] = (uint8_t)encoded;
    pixel[1] = (uint8_t)(encoded >> 8);
    pixel[2] = (uint8_t)(encoded >> 16);
    if (bytes_per_pixel == 4) {
        pixel[3] = (uint8_t)(encoded >> 24);
    }
}

void avlos_fb_fill_rect(const struct avlos_framebuffer *framebuffer,
                       uint32_t x, uint32_t y,
                       uint32_t width, uint32_t height, uint32_t rgb) {
    if (framebuffer == 0 || x >= framebuffer->width ||
        y >= framebuffer->height) {
        return;
    }

    uint64_t clipped_width = width;
    uint64_t clipped_height = height;
    if (clipped_width > framebuffer->width - x) {
        clipped_width = framebuffer->width - x;
    }
    if (clipped_height > framebuffer->height - y) {
        clipped_height = framebuffer->height - y;
    }

    for (uint64_t row = 0; row < clipped_height; ++row) {
        for (uint64_t column = 0; column < clipped_width; ++column) {
            avlos_fb_pixel(framebuffer, x + (uint32_t)column,
                          y + (uint32_t)row, rgb);
        }
    }
}

void avlos_fb_clear(const struct avlos_framebuffer *framebuffer, uint32_t rgb) {
    if (framebuffer == 0) {
        return;
    }
    avlos_fb_fill_rect(framebuffer, 0, 0,
                      (uint32_t)framebuffer->width,
                      (uint32_t)framebuffer->height, rgb);
}

