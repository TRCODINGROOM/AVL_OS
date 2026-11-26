# Start the AVL OS desktop on the first console for the live user.
if [ -z "$DISPLAY" ] && [ "$(tty)" = "/dev/tty1" ] && [ "$(id -un)" = "avl" ]; then
  exec startx /opt/avlos/session.sh -- -nolisten tcp vt1 >/tmp/startx.log 2>&1
fi
