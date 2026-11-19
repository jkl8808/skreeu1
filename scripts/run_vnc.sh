#!/bin/bash
# 启动 Xvfb + x11vnc + skreeu1
#   VNC_DISPLAY=:99  VNC_PORT=5900  （默认）
set -e
DISP="${VNC_DISPLAY:-:99}"
PORT="${VNC_PORT:-5900}"
NUM="${DISP#:}"
rm -f "/tmp/.X${NUM}-lock" "/tmp/.X11-unix/X${NUM}"
Xvfb "$DISP" -screen 0 480x320x24 -nolisten tcp >/tmp/xvfb.log 2>&1 &
sleep 1
x11vnc -display "$DISP" -forever -shared -nopw -rfbport "$PORT" -quiet >/tmp/x11vnc.log 2>&1 &
sleep 1
export DISPLAY="$DISP"
exec ./build/skreeu1 "$@"
