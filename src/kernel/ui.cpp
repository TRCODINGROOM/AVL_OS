#include <avlos/kernel.h>
#include <limine.h>

namespace {

constexpr uint32_t kBackground = 0x07111f;
constexpr uint32_t kPanel = 0x0d1b2c;
constexpr uint32_t kPanelLight = 0x11243a;
constexpr uint32_t kBorder = 0x213a50;
constexpr uint32_t kText = 0xf0f6fc;
constexpr uint32_t kMuted = 0x93a9bf;
constexpr uint32_t kAccent = 0x3de0c2;
constexpr uint32_t kBlue = 0x62a8ff;
constexpr uint32_t kWarning = 0xffb454;

const uint8_t kLetters[26][5] = {
    {0x7e, 0x11, 0x11, 0x11, 0x7e}, {0x7f, 0x49, 0x49, 0x49, 0x36},
    {0x3e, 0x41, 0x41, 0x41, 0x22}, {0x7f, 0x41, 0x41, 0x22, 0x1c},
    {0x7f, 0x49, 0x49, 0x49, 0x41}, {0x7f, 0x09, 0x09, 0x09, 0x01},
    {0x3e, 0x41, 0x49, 0x49, 0x7a}, {0x7f, 0x08, 0x08, 0x08, 0x7f},
    {0x00, 0x41, 0x7f, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3f, 0x01},
    {0x7f, 0x08, 0x14, 0x22, 0x41}, {0x7f, 0x40, 0x40, 0x40, 0x40},
    {0x7f, 0x02, 0x04, 0x02, 0x7f}, {0x7f, 0x04, 0x08, 0x10, 0x7f},
    {0x3e, 0x41, 0x41, 0x41, 0x3e}, {0x7f, 0x09, 0x09, 0x09, 0x06},
    {0x3e, 0x41, 0x51, 0x21, 0x5e}, {0x7f, 0x09, 0x19, 0x29, 0x46},
    {0x26, 0x49, 0x49, 0x49, 0x32}, {0x01, 0x01, 0x7f, 0x01, 0x01},
    {0x3f, 0x40, 0x40, 0x40, 0x3f}, {0x1f, 0x20, 0x40, 0x20, 0x1f},
    {0x7f, 0x20, 0x18, 0x20, 0x7f}, {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x03, 0x04, 0x78, 0x04, 0x03}, {0x61, 0x51, 0x49, 0x45, 0x43},
};

const uint8_t kDigits[10][5] = {
    {0x3e, 0x45, 0x49, 0x51, 0x3e}, {0x00, 0x41, 0x7f, 0x40, 0x00},
    {0x62, 0x51, 0x49, 0x49, 0x46}, {0x22, 0x41, 0x49, 0x49, 0x36},
    {0x18, 0x14, 0x12, 0x7f, 0x10}, {0x2f, 0x49, 0x49, 0x49, 0x31},
    {0x3e, 0x49, 0x49, 0x49, 0x32}, {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x26, 0x49, 0x49, 0x49, 0x3e},
};

const uint8_t *glyph_for(char character) {
    static const uint8_t blank[5] = {0, 0, 0, 0, 0};
    static const uint8_t dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
    static const uint8_t colon[5] = {0, 0x36, 0, 0x36, 0};
    static const uint8_t slash[5] = {0x60, 0x10, 0x08, 0x04, 0x03};
    static const uint8_t period[5] = {0, 0x60, 0x60, 0, 0};
    static const uint8_t plus[5] = {0x08, 0x08, 0x3e, 0x08, 0x08};
    static const uint8_t underscore[5] = {0x40, 0x40, 0x40, 0x40, 0x40};
    static const uint8_t greater[5] = {0x41, 0x22, 0x14, 0x08, 0};

    if (character >= 'a' && character <= 'z') {
        character = static_cast<char>(character - 'a' + 'A');
    }
    if (character >= 'A' && character <= 'Z') {
        return kLetters[character - 'A'];
    }
    if (character >= '0' && character <= '9') {
        return kDigits[character - '0'];
    }

    switch (character) {
        case '-': return dash;
        case ':': return colon;
        case '/': return slash;
        case '.': return period;
        case '+': return plus;
        case '_': return underscore;
        case '>': return greater;
        default: return blank;
    }
}

void draw_text(const struct avlos_framebuffer *framebuffer,
               uint32_t x, uint32_t y, const char *text,
               uint32_t color, uint32_t scale) {
    if (text == nullptr || scale == 0) {
        return;
    }
    uint32_t character_index = 0;
    while (text[character_index] != '\0') {
        const uint8_t *glyph = glyph_for(text[character_index]);
        for (uint32_t column = 0; column < 5; ++column) {
            for (uint32_t row = 0; row < 7; ++row) {
                if ((glyph[column] & (1u << (6u - row))) != 0) {
                    uint32_t px = x + (character_index * 6u + column) * scale;
                    uint32_t py = y + row * scale;
                    avlos_fb_fill_rect(framebuffer, px, py, scale, scale, color);
                }
            }
        }
        ++character_index;
    }
}

void draw_number(const struct avlos_framebuffer *framebuffer,
                 uint32_t x, uint32_t y, uint64_t value, uint32_t color) {
    char digits[24];
    uint32_t count = 0;
    do {
        digits[count++] = static_cast<char>('0' + (value % 10u));
        value /= 10u;
    } while (value != 0 && count < sizeof(digits) - 1u);
    digits[count] = '\0';

    for (uint32_t left = 0; left < count / 2u; ++left) {
        char temporary = digits[left];
        digits[left] = digits[count - left - 1u];
        digits[count - left - 1u] = temporary;
    }
    draw_text(framebuffer, x, y, digits, color, 1);
}

void draw_hex64(const struct avlos_framebuffer *framebuffer,
                uint32_t x, uint32_t y, uint64_t value, uint32_t color) {
    static const char hex[] = "0123456789ABCDEF";
    char output[19];
    output[0] = '0';
    output[1] = 'X';
    for (uint32_t index = 0; index < 16; ++index) {
        uint32_t shift = (15u - index) * 4u;
        output[index + 2u] = hex[(value >> shift) & 0x0fu];
    }
    output[18] = '\0';
    draw_text(framebuffer, x, y, output, color, 1);
}

void draw_card(const struct avlos_framebuffer *framebuffer,
               uint32_t x, uint32_t y, uint32_t width, uint32_t height,
               const char *title, uint32_t title_color) {
    avlos_fb_fill_rect(framebuffer, x, y, width, height, kPanel);
    avlos_fb_fill_rect(framebuffer, x, y, width, 2, kBorder);
    avlos_fb_fill_rect(framebuffer, x, y, 2, height, kBorder);
    avlos_fb_fill_rect(framebuffer, x + width - 2u, y, 2, height, kBorder);
    avlos_fb_fill_rect(framebuffer, x, y + height - 2u, width, 2, kBorder);
    draw_text(framebuffer, x + 18u, y + 16u, title, title_color, 1);
}

} // namespace

void avlos_ui_draw_boot_screen(const struct avlos_boot_info *boot_info) {
    if (boot_info == nullptr) {
        return;
    }
    const struct avlos_framebuffer *framebuffer = &boot_info->framebuffer;
    const uint32_t width = static_cast<uint32_t>(framebuffer->width);
    const uint32_t height = static_cast<uint32_t>(framebuffer->height);
    if (width < 160 || height < 120) {
        return;
    }

    avlos_fb_clear(framebuffer, kBackground);
    avlos_fb_fill_rect(framebuffer, 0, 0, width, 7, kAccent);

    const uint32_t margin = width >= 640 ? 48u : 16u;
    const uint32_t header_y = 54u;
    const int32_t center_x = static_cast<int32_t>(margin + 30u);
    const int32_t center_y = static_cast<int32_t>(header_y + 34u);
    const int32_t radius = 27;
    for (int32_t dy = -radius; dy <= radius; ++dy) {
        for (int32_t dx = -radius; dx <= radius; ++dx) {
            int32_t distance_squared = dx * dx + dy * dy;
            if (distance_squared <= radius * radius &&
                distance_squared >= (radius - 4) * (radius - 4)) {
                avlos_fb_pixel(framebuffer,
                              static_cast<uint32_t>(center_x + dx),
                              static_cast<uint32_t>(center_y + dy), kAccent);
            } else if (distance_squared <= (radius - 7) * (radius - 7) &&
                       distance_squared >= (radius - 9) * (radius - 9)) {
                avlos_fb_pixel(framebuffer,
                              static_cast<uint32_t>(center_x + dx),
                              static_cast<uint32_t>(center_y + dy), kBlue);
            }
        }
    }

    const uint32_t title_scale = width >= 640 ? 2u : 1u;
    draw_text(framebuffer, margin + 70u, header_y,
              "AVLOS", kText, title_scale);
    draw_text(framebuffer, margin + 70u, header_y + 30u,
              "AARAVVIVAANLEGEND-OS", kMuted, 1);
    draw_text(framebuffer, margin + 70u, header_y + 48u,
              "VERSION 0.1.0", kAccent, 1);

    const uint32_t content_width = width > margin * 2u
                                       ? width - margin * 2u
                                       : width;
    const uint32_t hero_y = 168u;
    const uint32_t hero_height = 170u;
    draw_card(framebuffer, margin, hero_y, content_width, hero_height,
              "BOOT STATUS", kAccent);
    draw_text(framebuffer, margin + 20u, hero_y + 48u,
              "GRAPHICAL KERNEL IS LIVE", kText, title_scale);
    draw_text(framebuffer, margin + 20u, hero_y + 88u,
              "LIMINE HANDED OFF CONTROL TO AVLOS", kMuted, 1);
    avlos_fb_fill_rect(framebuffer, margin + 20u, hero_y + 126u,
                      content_width > 40u ? content_width - 40u : 1u,
                      6u, kPanelLight);
    avlos_fb_fill_rect(framebuffer, margin + 20u, hero_y + 126u,
                      content_width > 40u ? content_width - 40u : 1u,
                      6u, kAccent);

    const uint32_t gap = 16u;
    const uint32_t card_width = content_width > gap
                                    ? (content_width - gap) / 2u
                                    : content_width;
    const uint32_t first_x = margin;
    const uint32_t second_x = margin + card_width + gap;
    const uint32_t first_y = hero_y + hero_height + 18u;
    const uint32_t card_height = 136u;
    const char *firmware = boot_info->firmware_type == LIMINE_FIRMWARE_TYPE_X86BIOS
                               ? "LEGACY BIOS"
                               : "UEFI";

    draw_card(framebuffer, first_x, first_y, card_width, card_height,
              "BOOT MODE", kBlue);
    draw_text(framebuffer, first_x + 18u, first_y + 52u, firmware, kText, 1);
    draw_text(framebuffer, first_x + 18u, first_y + 78u,
              "BIOS + UEFI", kMuted, 1);

    draw_card(framebuffer, second_x, first_y, card_width, card_height,
              "LANGUAGE ABI", kAccent);
    draw_text(framebuffer, second_x + 18u, first_y + 52u,
              "C / C++ / RUST", kText, 1);
    draw_text(framebuffer, second_x + 18u, first_y + 78u,
              "NASM / X86-64", kMuted, 1);

    const uint32_t second_y = first_y + card_height + 14u;
    draw_card(framebuffer, first_x, second_y, card_width, card_height,
              "MEMORY MAP", kWarning);
    draw_text(framebuffer, first_x + 18u, second_y + 52u,
              "REGION ENTRIES", kMuted, 1);
    draw_number(framebuffer, first_x + 18u, second_y + 78u,
                boot_info->memory_map_entries, kText);

    draw_card(framebuffer, second_x, second_y, card_width, card_height,
              "RUST C ABI", boot_info->rust_abi_ok ? kAccent : kWarning);
    draw_text(framebuffer, second_x + 18u, second_y + 52u,
              boot_info->rust_abi_ok ? "CHECK PASSED" : "CHECK FAILED",
              kText, 1);
    draw_hex64(framebuffer, second_x + 18u, second_y + 78u,
               boot_info->rust_probe, kMuted);

    const uint32_t footer_y = height > 28u ? height - 28u : 0u;
    draw_text(framebuffer, margin, footer_y,
              "LIVE: INPUT / NETWORK / MULTICORE", kMuted, 1);
}

void avlos_ui_draw_runtime_overlay(const struct avlos_boot_info *boot_info,
                                  const struct avlos_runtime_status *status) {
    if (boot_info == nullptr || status == nullptr) {
        return;
    }
    const struct avlos_framebuffer *framebuffer = &boot_info->framebuffer;
    const uint32_t width = static_cast<uint32_t>(framebuffer->width);
    const uint32_t height = static_cast<uint32_t>(framebuffer->height);
    if (width < 160 || height < 80) {
        return;
    }

    const uint32_t top = height > 58u ? height - 58u : 0u;
    avlos_fb_fill_rect(framebuffer, 0, top, width, height - top, kPanelLight);
    avlos_fb_fill_rect(framebuffer, 0, top, width, 2u, kAccent);
    uint32_t x = 20u;
    uint32_t y = top + 12u;
    draw_text(framebuffer, x, y, "CPUS", kAccent, 1);
    draw_number(framebuffer, x + 30u, y, status->cpu_count, kText);
    x += 90u;
    draw_text(framebuffer, x, y, "FREE PAGES", kMuted, 1);
    draw_number(framebuffer, x + 66u, y, status->free_frames, kText);
    x += 180u;
    draw_text(framebuffer, x, y, "TASKS", kMuted, 1);
    draw_number(framebuffer, x + 42u, y, status->task_count, kText);
    x += 120u;
    draw_text(framebuffer, x, y, "KEY", kMuted, 1);
    draw_number(framebuffer, x + 24u, y, status->keyboard_events, kText);
    x += 110u;
    draw_text(framebuffer, x, y, "MOUSE", kMuted, 1);
    draw_number(framebuffer, x + 36u, y, status->mouse_events, kText);
    if (width > 800u) {
        x += 100u;
        draw_text(framebuffer, x, y, "WORK", kAccent, 1);
        draw_number(framebuffer, x + 32u, y, status->cpu_work_total, kText);
    }

    x = 20u;
    y += 20u;
    draw_text(framebuffer, x, y, status->storage_ready ? "ATA READY" : "ATA NONE",
              status->storage_ready ? kAccent : kMuted, 1);
    x += 108u;
    draw_text(framebuffer, x, y, status->network_ready ? "NET READY" : "NET NONE",
              status->network_ready ? kAccent : kMuted, 1);
    x += 110u;
    draw_text(framebuffer, x, y, status->audio_ready ? "AUDIO READY" : "AUDIO NONE",
              status->audio_ready ? kAccent : kMuted, 1);
    x += 132u;
    draw_text(framebuffer, x, y, status->acpi_ready ? "ACPI READY" : "ACPI NONE",
              status->acpi_ready ? kAccent : kMuted, 1);
    x += 122u;
    draw_text(framebuffer, x, y, status->userspace_ready ? "RING 3 READY" : "RING 3 NONE",
              status->userspace_ready ? kAccent : kMuted, 1);

    const uint32_t cursor_size = 9u;
    avlos_fb_fill_rect(framebuffer, status->cursor_x, status->cursor_y,
                      cursor_size, 2u, kText);
    avlos_fb_fill_rect(framebuffer, status->cursor_x, status->cursor_y,
                      2u, cursor_size, kText);
}

