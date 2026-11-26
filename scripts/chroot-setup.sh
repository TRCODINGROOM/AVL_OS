#!/bin/bash
# Runs inside the new Ubuntu root: installs the kernel, casper (live boot), desktop libraries and services.
set -e
export DEBIAN_FRONTEND=noninteractive LC_ALL=C
cat > /etc/apt/sources.list <<'EOL'
deb http://archive.ubuntu.com/ubuntu noble main universe
deb http://archive.ubuntu.com/ubuntu noble-updates main universe
deb http://security.ubuntu.com/ubuntu noble-security main universe
EOL
echo 'exit 101' > /usr/sbin/policy-rc.d; chmod +x /usr/sbin/policy-rc.d
apt-get update -qq
apt-get install -y -qq --no-install-recommends casper initramfs-tools systemd-sysv udev dbus kmod
echo "RESUME=none" > /etc/initramfs-tools/conf.d/resume
apt-get install -y -qq --no-install-recommends linux-image-generic
apt-get install -y -qq --no-install-recommends network-manager wpasupplicant bluez rfkill iproute2 iputils-ping ca-certificates curl \
  xserver-xorg-core xserver-xorg-input-libinput xserver-xorg-video-vmware xserver-xorg-video-fbdev xserver-xorg-video-qxl xinit x11-xserver-utils openbox \
  open-vm-tools open-vm-tools-desktop pulseaudio pulseaudio-utils pulseaudio-module-bluetooth alsa-utils \
  libgtk-3-0t64 libnss3 libgbm1 libasound2t64 libxss1 libxtst6 libatk-bridge2.0-0t64 libcups2t64 libdrm2 libxkbcommon0 libx11-xcb1 libxcomposite1 libxdamage1 libxrandr2 libpango-1.0-0 libcairo2 \
  libgl1-mesa-dri libglx-mesa0 libegl1 xdg-utils fonts-inter fonts-dejavu-core fonts-liberation fonts-noto-color-emoji sudo
if [ "${WITH_FIRMWARE:-0}" = "1" ]; then apt-get install -y -qq --no-install-recommends linux-firmware; fi
systemctl enable NetworkManager bluetooth open-vm-tools >/dev/null 2>&1 || true
systemctl disable NetworkManager-wait-online systemd-networkd-wait-online >/dev/null 2>&1 || true
systemctl mask systemd-networkd-wait-online >/dev/null 2>&1 || true
fc-cache -f >/dev/null 2>&1 || true
apt-get clean
rm -rf /var/lib/apt/lists/* /var/cache/apt/archives/*.deb /usr/share/doc/* /usr/share/man/* /usr/share/info/* /tmp/*
find /usr/share/locale -mindepth 1 -maxdepth 1 ! -name 'en*' -exec rm -rf {} + 2>/dev/null || true
rm -f /usr/sbin/policy-rc.d
echo "chroot-setup done"
