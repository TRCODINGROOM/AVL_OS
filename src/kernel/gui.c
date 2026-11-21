#include <avlos/gui.h>
#include <avlos/acpi.h>
#include <avlos/audio.h>
#include <avlos/memory.h>
#include <avlos/network.h>
#include <avlos/scheduler.h>
#include <avlos/smp.h>
#include <avlos/storage.h>
#include <avlos/userspace.h>

static struct avlos_boot_info gui_boot_info;
static uint32_t cursor_x;
static uint32_t cursor_y;
static uint64_t keyboard_events;
static uint64_t mouse_events;

void gui_init(const struct avlos_boot_info *boot_info) {
    if (boot_info == 0) {
        return;
    }
    gui_boot_info = *boot_info;
    cursor_x = (uint32_t)(boot_info->framebuffer.width / 2u);
    cursor_y = (uint32_t)(boot_info->framebuffer.height / 2u);
    keyboard_events = 0;
    mouse_events = 0;
    gui_redraw();
}

void gui_handle_event(const struct input_event *event) {
    if (event == 0) {
        return;
    }
    if (event->type == INPUT_EVENT_KEY) {
        if (event->pressed) {
            ++keyboard_events;
            if (event->code == 0x57u) {
                acpi_reboot();
            } else if (event->code == 0x58u) {
                acpi_shutdown();
            } else if (event->code == 0x3bu) {
                audio_beep(880, 40);
            }
        }
    } else if (event->type == INPUT_EVENT_MOUSE) {
        ++mouse_events;
        int64_t next_x = (int64_t)cursor_x + event->dx;
        int64_t next_y = (int64_t)cursor_y + event->dy;
        if (next_x < 0) next_x = 0;
        if (next_y < 0) next_y = 0;
        if (next_x >= (int64_t)gui_boot_info.framebuffer.width) {
            next_x = (int64_t)gui_boot_info.framebuffer.width - 1;
        }
        if (next_y >= (int64_t)gui_boot_info.framebuffer.height) {
            next_y = (int64_t)gui_boot_info.framebuffer.height - 1;
        }
        cursor_x = (uint32_t)next_x;
        cursor_y = (uint32_t)next_y;
    }
}

void gui_redraw(void) {
    struct avlos_runtime_status status = {0};
    status.cpu_count = smp_online_cpu_count();
    status.free_frames = pmm_free_frames();
    status.task_count = scheduler_task_count();
    status.cpu_work_total = kernel_cpu_work_total();
    status.keyboard_events = keyboard_events;
    status.mouse_events = mouse_events;
    status.cursor_x = cursor_x;
    status.cursor_y = cursor_y;
    status.storage_ready = (uint8_t)storage_available();
    status.network_ready = (uint8_t)network_available();
    status.audio_ready = 1;
    status.acpi_ready = (uint8_t)acpi_available();
    status.userspace_ready = (uint8_t)userspace_available();
    avlos_ui_draw_boot_screen(&gui_boot_info);
    avlos_ui_draw_runtime_overlay(&gui_boot_info, &status);
}

void gui_process_events(void) {
    struct input_event event;
    uint32_t processed = 0;
    while (processed < 32u && input_pop(&event)) {
        gui_handle_event(&event);
        ++processed;
    }
    if (processed != 0) {
        gui_redraw();
    }
}

