#!/bin/bash
# Build the AVL OS live ISO (Ubuntu 24.04 base). Run as root on Ubuntu 24.04:  sudo bash scripts/build.sh
# Options (environment): COMP=xz|zstd (default xz), WITH_FIRMWARE=1 (add linux-firmware for real Wi-Fi/Bluetooth hardware)
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="${WORK:-/var/tmp/avlos-build}"; OUT="${OUT:-$ROOT/out}"; RFS="$WORK/rootfs"; COMP="${COMP:-xz}"
export DEBIAN_FRONTEND=noninteractive LC_ALL=C

echo "== Installing build tools"
apt-get update -qq
apt-get install -y -qq debootstrap squashfs-tools grub-pc-bin grub-efi-amd64-bin grub-common mtools xorriso dosfstools initramfs-tools-core zstd
command -v npm >/dev/null 2>&1 || apt-get install -y -qq nodejs npm

cleanup() { for m in dev/pts dev proc sys; do umount -l "$RFS/$m" 2>/dev/null || true; done; }
trap cleanup EXIT
rm -rf "$WORK"; mkdir -p "$WORK" "$OUT"

echo "== Creating the Ubuntu base system"
debootstrap --variant=minbase --arch=amd64 noble "$RFS" http://archive.ubuntu.com/ubuntu
mount -t proc none "$RFS/proc"; mount -t sysfs none "$RFS/sys"; mount --bind /dev "$RFS/dev"; mount --bind /dev/pts "$RFS/dev/pts"
cp /etc/resolv.conf "$RFS/etc/resolv.conf"
cp "$ROOT/scripts/chroot-setup.sh" "$RFS/root/chroot-setup.sh"; chmod +x "$RFS/root/chroot-setup.sh"
chroot "$RFS" env WITH_FIRMWARE="${WITH_FIRMWARE:-0}" /root/chroot-setup.sh
rm -f "$RFS/root/chroot-setup.sh"

echo "== Installing Electron (Chromium) and the AVL OS desktop"
mkdir -p "$WORK/el" && (cd "$WORK/el" && npm init -y >/dev/null && npm install electron@33.2.1 --no-audit --no-fund >/dev/null)
mkdir -p "$RFS/opt/avlos/app" "$RFS/opt/avlos/electron"
cp -r "$WORK/el/node_modules/electron/dist/." "$RFS/opt/avlos/electron/"
rm -f "$RFS/opt/avlos/electron/LICENSES.chromium.html"; find "$RFS/opt/avlos/electron/locales" -type f ! -name 'en-US.pak' -delete
for f in index.html main.js preload.js package.json; do cp "$ROOT/app/$f" "$RFS/opt/avlos/app/$f"; done
cp -a "$ROOT/overlay/." "$RFS/"
chmod +x "$RFS/opt/avlos/session.sh"; chmod 440 "$RFS/etc/sudoers.d/avlos"
cleanup; trap - EXIT

echo "== Building the bootable ISO"
ISO="$WORK/iso"; mkdir -p "$ISO/casper" "$ISO/boot/grub"
cp "$RFS"/boot/vmlinuz-*-generic "$ISO/casper/vmlinuz"; cp "$RFS"/boot/initrd.img-*-generic "$ISO/casper/initrd"
IS=$(stat -c %s "$ISO/casper/initrd"); echo "initrd size: $IS bytes"
[ "$IS" -lt 200000000 ] || { echo "ERROR: initrd too large"; exit 1; }
(unmkinitramfs "$ISO/casper/initrd" "$WORK/ird" 2>/dev/null || true); find "$WORK/ird" -path '*scripts/casper' | grep -q . || { echo "ERROR: casper missing from initrd"; exit 1; }
cp "$ROOT/iso/boot/grub/grub.cfg" "$ISO/boot/grub/grub.cfg"
if [ "$COMP" = "zstd" ]; then CO="-comp zstd -Xcompression-level 9 -b 256K"; else CO="-comp xz -b 1M"; fi
mksquashfs "$RFS" "$ISO/casper/filesystem.squashfs" $CO -noappend -e boot/initrd.img\* boot/vmlinuz\* >/dev/null
grub-mkrescue -o "$OUT/avlos.iso" "$ISO" -- -volid AVLOS >/dev/null 2>&1
ls -lh "$OUT/avlos.iso"; echo "== Done: $OUT/avlos.iso"
