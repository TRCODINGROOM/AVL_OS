# Safe hardware testing

1. Build the ISO and boot it in QEMU first. Try `make run` for BIOS and `make run-uefi` for UEFI.
2. Use a spare USB drive or a rewritable optical disc. Back up anything on the removable device before writing the ISO.
3. In the imaging program, verify the selected destination by its name, size, and removable-device status before starting. Do not select an internal system disk.
4. Use the firmware's one-time boot menu so the test image does not replace the normal boot order.
5. For BIOS testing, use hardware whose firmware still supports Legacy/CSM boot. For UEFI testing, use x86-64 UEFI.
6. If Secure Boot rejects the unsigned development bootloader, use a non-production test machine and temporarily disable Secure Boot there. Restore the setting after testing.
7. Keep the machine on stable power. A PS/2 keyboard can request a tone with F1, reboot with F11, and ACPI shutdown with F12. Firmware and hardware support varies; if the kernel is stuck, remove the test media and use the machine's normal recovery procedure.

The supplied build runs `limine bios-install` only on the generated ISO file under `build/`. It does not flash a USB drive or install a bootloader to a machine. Boot currently probes the primary-master ATA device but does not write to it; the QEMU run targets create and attach a disposable blank disk image under `build/`.

