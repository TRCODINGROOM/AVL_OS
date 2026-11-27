# AVL OS (Linux edition)

A bootable operating system with the AVL OS desktop: lock screen, windows, taskbar, Notes, Terminal, Files, Settings, the AVL Store and **Yadu**, a real Chromium-based web browser with tabs.

It boots from an ISO (Ubuntu 24.04 base, Linux kernel) straight into the AVL OS desktop. There is no other desktop underneath.

## What works
- **Yadu browser**: real Chromium engine, tabs, back/forward/reload, downloads to `~/Downloads`. YouTube, Google, WhatsApp Web and most sites work. (DRM video such as Netflix does not.)
- **AVL Store**: "Get" adds web apps to the desktop; they open in Yadu.
- **Settings**: real Wi-Fi (scan, connect with password), Bluetooth (scan, pair, connect), sound volume, network info, restart and shut down.
- **Terminal**: built-in commands, and anything else runs as a real Linux command.
- **Files**: browse the real home folder. **Notes** are also saved to `~/Documents`.

## Getting the ISO
1. Push this folder to a GitHub repository (the workflow is in `.github/workflows/build-iso.yml`).
2. Open the **Actions** tab, run **Build AVL OS ISO**, wait for the green check.
3. Download the `avlos-iso` artifact and extract it to get `avlos.iso`.

Or build it yourself on Ubuntu 24.04: `sudo bash scripts/build.sh` (needs about 12 GB free).

## Running it in VMware
New VM > Custom > "I will install the operating system later" > Linux, Ubuntu 64-bit. 4 GB RAM, 2 processors, no hard disk needed (or 1 GB). Attach `avlos.iso` as the CD/DVD and power on. `open-vm-tools` is included, so the screen resizes with the window.

Note: a virtual machine has no Wi-Fi hardware. Ethernet works through VMware's NAT. For Wi-Fi and Bluetooth, use a laptop (boot from a USB stick) or pass a USB Wi-Fi/Bluetooth adapter to the VM.

## Notes
- Everything is in RAM in live mode, so changes are lost when you shut down.
- Licenses: AVL OS code is GPL-2.0-only. Ubuntu, Linux, Chromium/Electron (MIT/BSD-style) and Inter (SIL OFL) keep their own licenses.
