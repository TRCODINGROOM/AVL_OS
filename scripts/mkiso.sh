#!/bin/bash
# Secure Boot capable ISO: Microsoft-signed shim -> Canonical-signed GRUB -> Canonical-signed kernel.
# usage: mkiso.sh ISO_TREE OUT_ISO   (needs shim-signed, grub-efi-amd64-signed, grub-pc-bin, mtools, xorriso)
set -e
ISO="$1"; OUT="$2"; R="${3:-}"; T=$(mktemp -d)  # R = rootfs dir holding shim-signed/grub-signed
SHIM=$R/usr/lib/shim/shimx64.efi.dualsigned; [ -f "$SHIM" ] || SHIM=$R/usr/lib/shim/shimx64.efi.signed
GRUBS=$R/usr/lib/grub/x86_64-efi-signed/grubx64.efi.signed
MM=$R/usr/lib/shim/mmx64.efi
CFG='search --no-floppy --file --set=root /casper/vmlinuz
set prefix=($root)/boot/grub
configfile ($root)/boot/grub/grub.cfg'
mkdir -p "$ISO/EFI/boot" "$ISO/boot/grub/x86_64-efi" "$ISO/boot/grub/i386-pc"
cp "$SHIM" "$ISO/EFI/boot/BOOTX64.EFI"; cp "$GRUBS" "$ISO/EFI/boot/grubx64.efi"; cp "$MM" "$ISO/EFI/boot/mmx64.efi"
echo "$CFG" > "$ISO/EFI/boot/grub.cfg"
# Canonical's signed GRUB looks for /boot/grub/x86_64-efi/grub.cfg (and EFI/ubuntu) on its own:
echo "$CFG" > "$ISO/boot/grub/x86_64-efi/grub.cfg"
mkdir -p "$ISO/EFI/ubuntu"; echo "$CFG" > "$ISO/EFI/ubuntu/grub.cfg"
# BIOS
cp -r /usr/lib/grub/i386-pc/*.mod /usr/lib/grub/i386-pc/*.lst "$ISO/boot/grub/i386-pc/" 2>/dev/null || true
grub-mkimage -O i386-pc-eltorito -o "$ISO/boot/grub/i386-pc/eltorito.img" -p /boot/grub biosdisk iso9660 normal configfile search linux echo ls part_gpt part_msdos
# EFI system partition image
dd if=/dev/zero of="$T/efi.img" bs=1M count=12 2>/dev/null; mkfs.vfat -F 16 "$T/efi.img" >/dev/null
mmd -i "$T/efi.img" ::EFI ::EFI/boot ::EFI/ubuntu
for f in BOOTX64.EFI grubx64.efi mmx64.efi grub.cfg; do mcopy -i "$T/efi.img" "$ISO/EFI/boot/$f" ::EFI/boot/; done
mcopy -i "$T/efi.img" "$ISO/EFI/ubuntu/grub.cfg" ::EFI/ubuntu/
xorriso -as mkisofs -r -V AVLOS -o "$OUT" -J -joliet-long -partition_offset 16 \
  --grub2-mbr /usr/lib/grub/i386-pc/boot_hybrid.img -append_partition 2 0xef "$T/efi.img" -appended_part_as_gpt \
  -c boot.catalog -b boot/grub/i386-pc/eltorito.img -no-emul-boot -boot-load-size 4 -boot-info-table --grub2-boot-info \
  -eltorito-alt-boot -e '--interval:appended_partition_2:::' -no-emul-boot "$ISO" >/dev/null 2>&1
rm -rf "$T"; ls -lh "$OUT"
