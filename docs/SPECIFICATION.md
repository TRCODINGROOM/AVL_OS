# AaravVivaanLegend-OS initial specification

## Project identity

- Name: AaravVivaanLegend-OS
- Short name: AVLOS
- Starter release: 0.1.0
- Architecture: x86-64
- Boot paths: Legacy BIOS and UEFI using a hybrid Limine ISO
- Kernel style: modular hybrid-monolithic kernel image
- Languages: C, C++, Rust, and x86-64 NASM Assembly with C ABI boundaries
- Intended targets: QEMU development and later physical-hardware experiments
- Initial interface: graphical Limine framebuffer dashboard

## 0.1.0 implementation scope

The initial implementation includes a physical frame allocator, a small slab heap, editing of the bootloader's four-level page tables, IDT/PIC/PIT handling, a minimal scheduler, AP startup and per-CPU worker tasks, a Ring 3 `int 0x80` demonstration, framebuffer GUI, PS/2 keyboard/mouse input, ATA PIO storage, RTL8139 Ethernet with basic ARP/ICMP, PC-speaker audio, and a limited ACPI shutdown/reboot path.

These are educational prototypes. The release does not claim a general-purpose userspace, filesystem, complete network stack, full PCI framework, complete ACPI AML implementation, or production device support. See [ARCHITECTURE.md](ARCHITECTURE.md) for operational details and limits.

## Build and test expectations

- The kernel must be freestanding and linked as one x86-64 ELF image.
- The boot ISO should support BIOS and UEFI firmware paths.
- The default development workflow is Linux or WSL2, using QEMU for first boot checks.
- Physical media should only be written after QEMU checks and with an explicitly selected spare device.

