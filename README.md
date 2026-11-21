# AaravVivaanLegend-OS (AVLOS) 0.1.0

AVLOS is an experimental x86-64 operating-system starter. This source tree builds a freestanding kernel and a BIOS+UEFI hybrid ISO using Limine. It keeps the kernel as one modular monolithic image and starts with a graphical framebuffer dashboard.

Version 0.1.0 adds working starter implementations for physical and heap allocation, four-level page-table edits, IDT/PIC/PIT interrupts, a small scheduler, per-CPU work tasks, a Ring 3 syscall demonstration, and basic devices. These are learning foundations; they are not a general-purpose or production-ready operating system.

## Included

- Limine requests for framebuffer, firmware type, memory map, HHDM, four-level paging, multiprocessor information, and ACPI RSDP.
- C kernel startup, C++20 framebuffer dashboard, Rust `no_std` C ABI checksum, and NASM x86-64 entry, interrupt, context-switch, and Ring 3 transition code.
- Physical frame free-list allocator and a small slab-style heap allocator.
- Page mapper for 4 KiB pages in Limine's existing four-level address space, including user mappings and NX where supported.
- Per-CPU GDT/TSS, IDT exception and legacy IRQ stubs, PIC/PIT support, and x2APIC timer/IPI vectors.
- Cooperative kernel scheduler with timer-assisted preemption and one pinned work task for each processor reported by Limine (up to 64 CPUs).
- A user-mode demo with a mapped code page and stack, using `int 0x80` for write, yield, and exit calls.
- PS/2 set-1 keyboard and basic three-byte mouse packets, primary-master ATA PIO sector I/O, PC-speaker beeps, and PCI-discovered RTL8139 Ethernet with a small ARP/IPv4 ICMP echo path.
- ACPI table checksum/lookup, `_S5_` shutdown parsing for common static AML packages, and ACPI reset-register reboot with keyboard-controller fallback.
- BIOS+UEFI hybrid ISO, QEMU BIOS/UEFI run targets, serial logging, and GDB pause targets.
- Pinned Limine 12.11.1 binary package verified by SHA-256 before use.

## Build prerequisites

Build in Linux or WSL2. The default Makefile uses Clang and LLD. Install GNU Make, Clang, LLD, NASM, xorriso, curl, tar, and QEMU. On Debian or Ubuntu:

~~~sh
sudo apt update
sudo apt install build-essential clang lld nasm xorriso curl ca-certificates qemu-system-x86 ovmf gdb
~~~

Install Rust with rustup and add the bare-metal target:

~~~sh
rustup target add x86_64-unknown-none
~~~

The first build downloads the pinned Limine binary package and checks its SHA-256 digest. The project does not write to a physical disk during build or QEMU runs.

## Build

From the project directory:

~~~sh
make all
~~~

Build outputs:

- `build/avlos-kernel.elf` — kernel ELF with debug symbols.
- `build/avlos-0.1.0.iso` — BIOS+UEFI hybrid boot image.
- `build/avlos-test-disk.img` — a blank 16 MiB disk created by QEMU run targets for ATA PIO detection.

Select the LLVM toolchain explicitly with `make TOOLCHAIN=llvm all`. If an x86-64 ELF GCC toolchain is installed, use `make TOOLCHAIN=gnu all`. The C and C++ side is freestanding; the Rust library is built for `x86_64-unknown-none` and linked as a static library.

## Run in QEMU

Boot using emulated BIOS:

~~~sh
make run
~~~

Boot the same image through OVMF UEFI firmware:

~~~sh
make run-uefi OVMF_CODE=/path/to/OVMF_CODE.fd OVMF_VARS=/path/to/OVMF_VARS.fd
~~~

The run targets attach an empty IDE disk and an emulated RTL8139 NIC on QEMU's user-mode network. The UEFI target copies the OVMF variable template to `build/OVMF_VARS.fd` before launch. The display is graphical; kernel status appears on the serial console.

Keyboard controls: **F1** plays a short PC-speaker tone, **F11** requests reboot, and **F12** requests ACPI shutdown. Power controls depend on firmware tables and hardware support.

## Debug

Start QEMU paused with a GDB stub on port 1234:

~~~sh
make debug
~~~

In a second terminal:

~~~sh
gdb build/avlos-kernel.elf
~~~

Then attach and stop at the kernel entry:

~~~text
target remote :1234
hbreak kernel_main
continue
~~~

Use `make debug-uefi` for the UEFI path. The QEMU targets leave the monitor disabled and use the serial terminal for kernel output.

## Physical hardware

Read [docs/HARDWARE-TESTING.md](docs/HARDWARE-TESTING.md) before testing. Verify both firmware paths in QEMU first, then use a spare removable device and the firmware's one-time boot menu. Keep a recovery path available. This development image is unsigned and is not enrolled for Secure Boot; follow your machine's normal test-image policy.

## Current limits

- The GUI is a framebuffer dashboard with a mouse cursor and live counters; it has no window manager, compositing, or graphics acceleration.
- The heap is deliberately small: slab classes through 2 KiB and single-page requests up to 4032 bytes, leaving one page for allocator metadata. There is no virtual-memory area manager, paging reclaim, separate process address space, or TLB shootdown.
- Ring 3 is a single mapped demo sharing the kernel's address space. It is not a secure process model; syscall pointer validation, process lifecycle, ELF loading, and isolation are absent.
- Scheduling is a minimal round-robin prototype. Per-CPU worker tasks yield cooperatively; timer scheduling is enabled where the legacy PIT or x2APIC timer is available. The x2APIC timer divisor is a prototype setting and is not calibrated.
- Storage supports primary-master ATA PIO sector operations only; there is no filesystem or partition manager.
- Networking is an RTL8139 learning driver and a static QEMU-style IPv4 address with basic ARP and ICMP echo handling. There is no DHCP, TCP, socket API, or general PCI driver framework.
- Input supports legacy PS/2 devices only. Audio is the PC speaker, not an audio codec or sound card driver.
- ACPI support reads a limited set of tables and recognizes common static `_S5_` AML packages. It does not implement a full AML interpreter or the broader ACPI device/power model.
- Broad hardware coverage is not claimed. Test in QEMU first; physical devices may need different drivers, firmware settings, and interrupt routing.

## Project map

~~~text
include/avlos/       C interfaces shared between subsystems
src/arch/x86_64/    paging, IDT/GDT setup, SMP, and NASM entry/interrupt code
src/boot/           Limine protocol requests
src/drivers/        ATA, PCI, PS/2, RTL8139, and PC speaker prototypes
src/firmware/       ACPI table and power-control prototype
src/kernel/         startup, memory, scheduler, GUI bridge, networking, userspace
src/rust/           no_std Rust C-ABI library
docs/               specification, architecture, and hardware guidance
~~~

## Project notes

- Initial specification: [docs/SPECIFICATION.md](docs/SPECIFICATION.md)
- Architecture and limitations: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- Hardware testing: [docs/HARDWARE-TESTING.md](docs/HARDWARE-TESTING.md)
- Third-party components: [THIRD_PARTY.md](THIRD_PARTY.md)
- Limine protocol: <https://github.com/Limine-Bootloader/limine-protocol>
- Limine hybrid ISO use: <https://github.com/Limine-Bootloader/Limine/blob/v12.x/USAGE.md>
