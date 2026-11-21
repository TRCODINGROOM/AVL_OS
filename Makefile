SHELL := /bin/sh

TOOLCHAIN ?= llvm
NASM ?= nasm
XORRISO ?= xorriso
QEMU ?= qemu-system-x86_64
HOST_CC ?= cc

ifeq ($(TOOLCHAIN),llvm)
CC := clang
CXX := clang++
TARGET_FLAGS := --target=x86_64-unknown-none-elf
LINKER_FLAGS := -fuse-ld=lld
else ifeq ($(TOOLCHAIN),gnu)
CC := x86_64-elf-gcc
CXX := x86_64-elf-g++
TARGET_FLAGS :=
LINKER_FLAGS :=
else
$(error TOOLCHAIN must be llvm or gnu)
endif

ARCH_FLAGS := -m64 -march=x86-64 -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2
COMMON_FLAGS := $(TARGET_FLAGS) $(ARCH_FLAGS) -O2 -g -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections -MMD -MP -Wall -Wextra
CFLAGS := $(COMMON_FLAGS) -std=c11
CXXFLAGS := $(COMMON_FLAGS) -std=c++20 -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit

BUILD := build
OBJ := $(BUILD)/obj
RUST_TARGET := $(BUILD)/rust-target
RUST_LIB := $(RUST_TARGET)/x86_64-unknown-none/release/libavlos_rust.a
KERNEL := $(BUILD)/avlos-kernel.elf
ISO := $(BUILD)/avlos-0.1.0.iso
DISK := $(BUILD)/avlos-test-disk.img
ISO_ROOT := $(BUILD)/iso_root
LIMINE_DIR := $(BUILD)/deps/limine-binary
LIMINE_STAMP := $(BUILD)/deps/limine-binary/.avlos-ready
LIMINE_TOOL := $(LIMINE_DIR)/limine

OVMF_CODE ?= /usr/share/OVMF/OVMF_CODE.fd
OVMF_VARS ?= /usr/share/OVMF/OVMF_VARS.fd
QEMU_MEMORY ?= 512M
QEMU_CPUS ?= 2

C_SOURCES := \
	src/boot/limine_requests.c \
	src/arch/x86_64/interrupts.c \
	src/arch/x86_64/paging.c \
	src/arch/x86_64/smp.c \
	src/drivers/ata_pio.c \
	src/drivers/pci.c \
	src/drivers/pc_speaker.c \
	src/drivers/ps2.c \
	src/drivers/rtl8139.c \
	src/firmware/acpi.c \
	src/kernel/gui.c \
	src/kernel/main.c \
	src/kernel/memory.c \
	src/kernel/network.c \
	src/kernel/runtime.c \
	src/kernel/scheduler.c \
	src/kernel/serial.c \
	src/kernel/userspace.c
CXX_SOURCES := src/kernel/ui.cpp
ASM_SOURCES := \
	src/arch/x86_64/entry.asm \
	src/arch/x86_64/interrupts.asm \
	src/arch/x86_64/io.asm \
	src/arch/x86_64/user_entry.asm

OBJECTS := \
	$(patsubst src/%.c,$(OBJ)/%.o,$(C_SOURCES)) \
	$(patsubst src/%.cpp,$(OBJ)/%.o,$(CXX_SOURCES)) \
	$(patsubst src/%.asm,$(OBJ)/%.o,$(ASM_SOURCES))
DEPS := $(OBJECTS:.o=.d)

-include $(DEPS)

.PHONY: all kernel iso limine-deps run run-uefi debug debug-uefi clean

all: iso

kernel: $(KERNEL)

iso: $(ISO)

$(BUILD):
	mkdir -p $(BUILD)

$(DISK): | $(BUILD)
	truncate -s 16M $@

limine-deps: $(LIMINE_STAMP)

$(LIMINE_STAMP):
	bash ./scripts/get-limine.sh
	touch $@

$(LIMINE_TOOL): $(LIMINE_STAMP)
	$(MAKE) -C $(LIMINE_DIR) CC="$(HOST_CC)"

$(OBJ)/%.o: src/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

$(OBJ)/%.o: src/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Iinclude -c $< -o $@

$(OBJ)/%.o: src/%.asm
	mkdir -p $(dir $@)
	$(NASM) -f elf64 -g -F dwarf $< -o $@

$(RUST_LIB): src/rust/Cargo.toml src/rust/Cargo.lock src/rust/src/lib.rs .cargo/config.toml rust-toolchain.toml
	cargo build --locked --release --manifest-path src/rust/Cargo.toml --target x86_64-unknown-none --target-dir $(RUST_TARGET)

$(KERNEL): $(OBJECTS) $(RUST_LIB) linker.ld
	mkdir -p $(BUILD)
	$(CXX) $(TARGET_FLAGS) $(ARCH_FLAGS) $(LINKER_FLAGS) -nostdlib -static -no-pie \
		-Wl,-T,linker.ld -Wl,-z,max-page-size=0x1000 -Wl,-z,noexecstack \
		-Wl,--gc-sections -Wl,--build-id=none -Wl,-Map,$(BUILD)/avlos-kernel.map \
		-o $@ $(OBJECTS) $(RUST_LIB)

$(ISO): $(KERNEL) limine.conf $(LIMINE_STAMP) $(LIMINE_TOOL)
	rm -rf $(ISO_ROOT)
	mkdir -p $(ISO_ROOT)/boot $(ISO_ROOT)/EFI/BOOT
	cp $(KERNEL) $(ISO_ROOT)/boot/avlos-kernel.elf
	cp limine.conf $(ISO_ROOT)/limine.conf
	cp $(LIMINE_DIR)/limine-bios.sys $(ISO_ROOT)/boot/limine-bios.sys
	cp $(LIMINE_DIR)/limine-bios-cd.bin $(ISO_ROOT)/boot/limine-bios-cd.bin
	cp $(LIMINE_DIR)/limine-uefi-cd.bin $(ISO_ROOT)/boot/limine-uefi-cd.bin
	cp $(LIMINE_DIR)/BOOTX64.EFI $(ISO_ROOT)/EFI/BOOT/BOOTX64.EFI
	$(XORRISO) -as mkisofs -R -r -J \
		-b boot/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table \
		-hfsplus -apm-block-size 2048 --efi-boot boot/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(ISO_ROOT) -o $@
	$(LIMINE_TOOL) bios-install $@

run: $(ISO) $(DISK)
	$(QEMU) -m $(QEMU_MEMORY) -smp $(QEMU_CPUS) -cdrom $(ISO) -drive file=$(DISK),format=raw,if=ide,index=0 -netdev user,id=net0 -device rtl8139,netdev=net0 -boot d -no-reboot -no-shutdown -serial stdio -monitor none

run-uefi: $(ISO) $(DISK)
	test -f "$(OVMF_CODE)" || (echo "Set OVMF_CODE to the OVMF_CODE.fd path"; exit 1)
	test -f "$(OVMF_VARS)" || (echo "Set OVMF_VARS to the OVMF_VARS.fd path"; exit 1)
	cp "$(OVMF_VARS)" $(BUILD)/OVMF_VARS.fd
	$(QEMU) -m $(QEMU_MEMORY) -smp $(QEMU_CPUS) \
		-drive if=pflash,format=raw,readonly=on,file="$(OVMF_CODE)" \
		-drive if=pflash,format=raw,file=$(BUILD)/OVMF_VARS.fd \
		-cdrom $(ISO) -drive file=$(DISK),format=raw,if=ide,index=0 \
		-netdev user,id=net0 -device rtl8139,netdev=net0 \
		-boot d -no-reboot -no-shutdown -serial stdio -monitor none

debug: $(ISO) $(DISK)
	$(QEMU) -m $(QEMU_MEMORY) -smp $(QEMU_CPUS) -cdrom $(ISO) -drive file=$(DISK),format=raw,if=ide,index=0 -netdev user,id=net0 -device rtl8139,netdev=net0 -boot d -no-reboot -no-shutdown -serial stdio -monitor none -S -s

debug-uefi: $(ISO) $(DISK)
	test -f "$(OVMF_CODE)" || (echo "Set OVMF_CODE to the OVMF_CODE.fd path"; exit 1)
	test -f "$(OVMF_VARS)" || (echo "Set OVMF_VARS to the OVMF_VARS.fd path"; exit 1)
	cp "$(OVMF_VARS)" $(BUILD)/OVMF_VARS.fd
	$(QEMU) -m $(QEMU_MEMORY) -smp $(QEMU_CPUS) \
		-drive if=pflash,format=raw,readonly=on,file="$(OVMF_CODE)" \
		-drive if=pflash,format=raw,file=$(BUILD)/OVMF_VARS.fd \
		-cdrom $(ISO) -drive file=$(DISK),format=raw,if=ide,index=0 \
		-netdev user,id=net0 -device rtl8139,netdev=net0 \
		-boot d -no-reboot -no-shutdown -serial stdio -monitor none -S -s

clean:
	rm -rf $(BUILD)

