#!/bin/sh
# X session for AVL OS: window manager (for fullscreen), VMware helpers, then the AVL OS desktop.
xsetroot -solid '#070b11' 2>/dev/null
xset s off -dpms 2>/dev/null
openbox --config-file /opt/avlos/openbox-rc.xml &
(vmware-user-suid-wrapper >/dev/null 2>&1 &)
export AVL_SOFT=1 ELECTRON_DISABLE_SECURITY_WARNINGS=1
cd /opt/avlos/app
/opt/avlos/electron/electron . --disable-dev-shm-usage >/tmp/avlos.log 2>&1 || /opt/avlos/electron/electron . --no-sandbox --disable-dev-shm-usage >>/tmp/avlos.log 2>&1
