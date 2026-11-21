#include <avlos/acpi.h>
#include <avlos/arch_x86_64.h>
#include <avlos/audio.h>
#include <avlos/gui.h>
#include <avlos/input.h>
#include <avlos/interrupts.h>
#include <avlos/kernel.h>
#include <avlos/limine_requests.h>
#include <avlos/memory.h>
#include <avlos/network.h>
#include <avlos/paging.h>
#include <avlos/scheduler.h>
#include <avlos/smp.h>
#include <avlos/storage.h>
#include <avlos/userspace.h>

#define RUST_AVLOS_FNV1A64 0x18de738b5f99dfcaULL
#define MAX_CPUS 64u

static volatile uint64_t cpu_work[MAX_CPUS];

uint64_t kernel_cpu_work_total(void) {
    uint64_t total = 0;
    for (uint32_t cpu = 0; cpu < MAX_CPUS; ++cpu) {
        total += __atomic_load_n(&cpu_work[cpu], __ATOMIC_RELAXED);
    }
    return total;
}

static void serial_write_u32(uint32_t value) {
    char digits[11];
    uint32_t count = 0;
    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0 && count < sizeof(digits));
    while (count != 0) {
        char text[2] = {digits[--count], '\0'};
        avlos_serial_write(text);
    }
}

static void kernel_stop(const char *reason) {
    avlos_serial_write("AVLOS: HALT: ");
    avlos_serial_write(reason);
    avlos_serial_write("\r\n");
    avlos_arch_halt();
}

static void service_task(void *argument) {
    (void)argument;
    uint64_t last_redraw = 0;
    for (;;) {
        network_poll();
        gui_process_events();
        uint64_t now = interrupt_ticks();
        if (now - last_redraw >= 10u) {
            last_redraw = now;
            gui_redraw();
        }
        scheduler_yield();
    }
}

static void userspace_task(void *argument) {
    (void)argument;
    userspace_enter_demo();
}

static void processor_worker(void *argument) {
    uint32_t cpu = (uint32_t)(uintptr_t)argument;
    if (cpu >= MAX_CPUS) {
        scheduler_exit_current();
    }
    for (;;) {
        for (uint32_t batch = 0; batch < 4096u; ++batch) {
            __atomic_add_fetch(&cpu_work[cpu], 1, __ATOMIC_RELAXED);
            __asm__ volatile("pause");
        }
        scheduler_yield();
    }
}

void kernel_main(void) {
    avlos_serial_initialize();
    avlos_serial_write("\r\nAaravVivaanLegend-OS 0.1.0\r\n");
    avlos_serial_write("C kernel entry reached through Limine.\r\n");

    if (!LIMINE_BASE_REVISION_SUPPORTED(avlos_limine_base_revision)) {
        kernel_stop("Limine base revision 6 is unavailable");
    }

    struct limine_framebuffer_response *framebuffer_response =
        avlos_framebuffer_request.response;
    struct limine_memmap_response *memory_map = avlos_memmap_request.response;
    struct limine_hhdm_response *hhdm_response = avlos_hhdm_request.response;
    if (framebuffer_response == 0 || framebuffer_response->framebuffer_count == 0 ||
        framebuffer_response->framebuffers == 0) {
        kernel_stop("no framebuffer was supplied");
    }
    if (memory_map == 0 || hhdm_response == 0) {
        kernel_stop("memory map or HHDM response is missing");
    }

    struct limine_framebuffer *source = framebuffer_response->framebuffers[0];
    if (source == 0 || source->address == 0 ||
        source->memory_model != LIMINE_FRAMEBUFFER_RGB ||
        (source->bpp != 24 && source->bpp != 32)) {
        kernel_stop("unsupported framebuffer format");
    }

    if (memory_init(memory_map, hhdm_response->offset) != 0) {
        kernel_stop("physical memory allocator initialization failed");
    }
    if (paging_init(hhdm_response->offset) != 0) {
        kernel_stop("current page-table root is unavailable");
    }
    (void)smp_prepare(avlos_mp_request.response);
    interrupts_init();

    struct limine_rsdp_response *rsdp_response = avlos_rsdp_request.response;
    int acpi_status = acpi_init(rsdp_response != 0 ? rsdp_response->address : 0);
    input_init();
    storage_init();
    network_init();
    audio_init();

    struct avlos_boot_info boot_info = {0};
    boot_info.framebuffer.address = (volatile uint8_t *)source->address;
    boot_info.framebuffer.width = source->width;
    boot_info.framebuffer.height = source->height;
    boot_info.framebuffer.pitch = source->pitch;
    boot_info.framebuffer.bpp = source->bpp;
    boot_info.framebuffer.memory_model = source->memory_model;
    boot_info.framebuffer.red_mask_size = source->red_mask_size;
    boot_info.framebuffer.red_mask_shift = source->red_mask_shift;
    boot_info.framebuffer.green_mask_size = source->green_mask_size;
    boot_info.framebuffer.green_mask_shift = source->green_mask_shift;
    boot_info.framebuffer.blue_mask_size = source->blue_mask_size;
    boot_info.framebuffer.blue_mask_shift = source->blue_mask_shift;
    boot_info.memory_map_entries = memory_map->entry_count;
    if (avlos_firmware_type_request.response != 0) {
        boot_info.firmware_type =
            avlos_firmware_type_request.response->firmware_type;
    }

    static const uint8_t rust_probe_input[] = {'A', 'L', 'O', 'S'};
    boot_info.rust_probe =
        avlos_rust_fnv1a64(rust_probe_input, sizeof(rust_probe_input));
    boot_info.rust_abi_ok = (boot_info.rust_probe == RUST_AVLOS_FNV1A64);

    int userspace_status = userspace_prepare_demo();
    gui_init(&boot_info);
    if (scheduler_init() != 0) {
        kernel_stop("scheduler initialization failed");
    }
    if (scheduler_create(service_task, 0, smp_bsp_cpu()) < 0) {
        kernel_stop("could not create BSP service task");
    }
    if (userspace_status &&
        scheduler_create(userspace_task, 0, smp_bsp_cpu()) < 0) {
        kernel_stop("could not create user-mode demonstration task");
    }
    for (uint32_t cpu = 0; cpu < smp_cpu_count() && cpu < MAX_CPUS; ++cpu) {
        if (scheduler_create(processor_worker, (void *)(uintptr_t)cpu, cpu) < 0) {
            kernel_stop("could not create a per-processor worker");
        }
    }

    uint32_t online = smp_start_aps();
    if (smp_x2apic_available()) {
        smp_local_timer_init();
    }
    avlos_serial_write("AVLOS memory, paging, interrupts, scheduler, input and GUI ready.\r\n");
    avlos_serial_write(storage_available() ? "ATA PIO device detected.\r\n"
                                          : "No primary-master ATA device detected.\r\n");
    avlos_serial_write(network_available() ? "RTL8139 network interface ready.\r\n"
                                          : "RTL8139 network interface not present.\r\n");
    avlos_serial_write(acpi_status == 0 ? "ACPI tables parsed.\r\n"
                                       : "ACPI power controls unavailable.\r\n");
    avlos_serial_write(userspace_status ? "Ring 3 demo prepared (int 0x80).\r\n"
                                       : "Ring 3 demo could not be mapped.\r\n");
    avlos_serial_write("Processors online: ");
    serial_write_u32(online);
    avlos_serial_write("; per-CPU work tasks are starting.\r\n");

    gui_redraw();
    scheduler_start();
}
