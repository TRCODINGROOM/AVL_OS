#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
DEPS_DIR="$ROOT_DIR/build/deps"
PACKAGE_DIR="$DEPS_DIR/limine-binary"
ARCHIVE="$DEPS_DIR/limine-binary.tar.gz"
VERSION="12.11.1"
SHA256="953aba25c23abc6d6b4e85e20ec8b9f3a8605450a26997103fa3fddf00e658e6"
URL="https://github.com/Limine-Bootloader/Limine/releases/download/v$VERSION/limine-binary.tar.gz"

if [ -f "$PACKAGE_DIR/limine-bios.sys" ] &&
   [ -f "$PACKAGE_DIR/limine-bios-cd.bin" ] &&
   [ -f "$PACKAGE_DIR/limine-uefi-cd.bin" ] &&
   [ -f "$PACKAGE_DIR/BOOTX64.EFI" ]; then
    exit 0
fi

mkdir -p "$DEPS_DIR"
if [ ! -f "$ARCHIVE" ]; then
    command -v curl >/dev/null 2>&1 || {
        echo "curl is required to download the pinned Limine package." >&2
        exit 1
    }
    curl --fail --location --retry 3 --silent --show-error \
        --output "$ARCHIVE" "$URL"
fi

command -v sha256sum >/dev/null 2>&1 || {
    echo "sha256sum is required to verify the Limine package." >&2
    exit 1
}
printf '%s  %s\n' "$SHA256" "$ARCHIVE" | sha256sum --check -
tar -xzf "$ARCHIVE" -C "$DEPS_DIR"

test -f "$PACKAGE_DIR/limine-bios.sys"
test -f "$PACKAGE_DIR/limine-bios-cd.bin"
test -f "$PACKAGE_DIR/limine-uefi-cd.bin"
test -f "$PACKAGE_DIR/BOOTX64.EFI"

