# AVLOS 0.1.0 architecture

AVLOS is one higher-half x86-64 kernel image, split into modules with C ABI boundaries. Limine supplies the first-stage BIOS or UEFI boot and boot-time resources. This release is a subsystem prototype, not a complete process-isolated operating system.

## Startup path

1. BIOS or UEFI starts Limine. `limine.conf` requests the kernel and framebuffer mode.
2. Limine supplies framebuffer, memory-map, HHDM, paging-mode, multiprocessor, firmware-type, and ACPI RSDP responses.
3. NASM `_start` runs with interrupts disabled, installs an initial kernel stack, aligns the stack for the x86-64 System V ABI, and calls `kernel_main`.
4. The kernel builds a physical-page free list from usable Limine memory, records the active CR3, and enables NX if the processor supports it.
5. The BSP establishes its GS-based CPU-local identity, loads a per-CPU GDT/TSS and the shared IDT, then initializes the PIT, PS/2 devices, ATA, PCI/RTL8139, PC speaker, and ACPI subset.
6. The kernel creates a 4 KiB user code and stack mapping, draws the C++ framebuffer dashboard, creates the service and per-CPU tasks, and starts the Limine-reported application processors.
7. Tasks run on the BSP and APs. The Ring 3 demo invokes `int 0x80` for write, yield, and exit. Device events are collected in interrupt handlers and consumed by the BSP service task.

## Language boundaries

| Area | Language | Public boundary |
| --- | --- | --- |
| Boot flow, memory management, scheduler, drivers, services | C11 | `include/avlos/*.h` |
| Framebuffer dashboard | C++20 | `avlos_ui_draw_boot_screen`, `avlos_ui_draw_runtime_overlay` |
| No-std helper library | Rust | `avlos_rust_fnv1a64` in the C ABI |
| Entry, port/MSR I/O, GDT reload, context switch, ISR stubs, Ring 3 IRET | NASM x86-64 | symbols declared in `include/avlos/arch_x86_64.h` and `interrupts.h` |

The C++ and Rust code do not use hosted runtimes. The Makefile links all parts into the same freestanding ELF image.

## Main subsystem contracts

| Subsystem | Entry points | Prototype behavior |
| --- | --- | --- |
| Physical/heap memory | `memory_init`, `pmm_alloc_frame`, `kmalloc` | Free frames are linked in HHDM memory; fixed-size slab classes and one-page allocations are available. |
| Paging | `paging_init`, `page_map`, `page_unmap`, `page_translate` | Edits the Limine-supplied 4-level root; supports 4 KiB leaf mappings. No separate address-space builder or TLB shootdown. |
| Interrupts | `interrupts_init`, `interrupt_dispatch` | Loads per-CPU GDT/TSS, shared IDT, remaps the legacy PIC, and programs the PIT. x2APIC vectors are used when the Limine MP response enables x2APIC. |
| SMP/scheduling | `smp_prepare`, `smp_start_aps`, `scheduler_create` | Starts APs through Limine and pins a yielding work task to each reported CPU. Round-robin run selection is small and global; this is not a real-time scheduler. |
| Userspace | `userspace_prepare_demo`, `syscall_dispatch` | Maps one demo code page and one stack into the current page tables; `int 0x80` offers write, yield, and exit. Kernel pages remain in the shared address space. |
| Input/GUI | `input_init`, `input_pop`, `gui_process_events` | PS/2 set-1 keyboard and three-byte mouse packets feed a small event ring. The C++ dashboard draws directly to the Limine framebuffer. |
| Storage | `storage_init`, `ata_read_sector`, `ata_write_sector` | Primary-master ATA PIO identify and single-sector LBA28 transfers. |
| Network | `network_init`, `network_poll` | Finds RTL8139 by PCI ID; basic Ethernet ARP and IPv4 ICMP echo on `10.0.2.15`. |
| Audio | `audio_init`, `audio_beep` | PC speaker tone via PIT channel 2. |
| ACPI | `acpi_init`, `acpi_shutdown`, `acpi_reboot` | Checks root/FADT/DSDT checksums, reads PM1 control/reset fields, parses common static `_S5_` packages, and writes shutdown/reset controls. |

## Boot and memory assumptions

- Architecture: x86-64 long mode, loaded as a higher-half ELF at `0xffffffff80000000`.
- Paging: Limine is asked for four-level paging; AVLOS edits the active CR3 and relies on Limine's HHDM for physical memory access.
- Physical allocation: usable memory-map frames are reserved as a free list. The bootloader-reclaimable and ACPI-reclaimable ranges are not recycled yet.
- Framebuffer: Limine RGB framebuffer, 24-bit or 32-bit pixels. Rendering targets the configured mode; there is no mode-setting driver.
- Per-CPU state: up to 64 processor records using Limine's MP response and IA32_GS_BASE.
- User address: the demo maps code at `0x00400000` and a stack page at `0x007ff000` into the current root.
- Device assumptions: legacy PIC/PIT, PS/2 ports, primary IDE ports, PCI configuration mechanism #1, and RTL8139 I/O BAR.

## Safety and completeness boundaries

- The page mapper changes permissions in the one shared root. There is no process isolation, page-table teardown, mapping ownership tracking, or inter-processor TLB invalidation.
- The heap supports small objects and up to 4032-byte requests. It has no page-run allocator, guard pages, or double-free detection.
- The scheduler uses a single lock for run selection and simple per-CPU affinity. Timer calibration, blocking/wakeup primitives, task migration, and fairness accounting are unfinished.
- The x2APIC timer uses a prototype initial count rather than frequency calibration. Tasks still voluntarily yield to keep the per-CPU worker path moving.
- Device support is limited to the devices and modes above. ATA has no filesystem; network has no DHCP or TCP; audio does not drive an audio codec.
- The ACPI code is not an AML interpreter and does not discover the complete namespace or power-management model. Shutdown depends on firmware's static `_S5_` package and PM1 register layout.
- QEMU is the first intended test target. Physical hardware behavior is not yet verified.

ACPI table layouts and AML package-length rules are referenced against the [UEFI Forum ACPI Specification 6.6, Software Programming Model](https://uefi.org/specs/ACPI/6.6/05_ACPI_Software_Programming_Model.html).
