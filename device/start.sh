#!/bin/sh
# 让 skreeu1 接管屏幕，同时保留 lmd / unisrv / MQTT / Moonraker 等官方服务。
#
# 关键点：**只替换 lmd 的 gui 子进程，不重启 lmd/unisrv**。
# 之前那种 kill lmd + 重新启动的做法会把 DRM 显示控制器留在关闭状态，
# 结果就是触摸有反应、画面也在更新，但屏幕一直黑。
#
# 用法（在打印机上）：
#   /userdata/skreeu1/start.sh
#   SKREEU1_READONLY=1 /userdata/skreeu1/start.sh   # 只读模式
#
# 恢复原厂界面：/userdata/skreeu1/stop.sh
set -e

GUI=/usr/bin/gui
BIN=/userdata/skreeu1/skreeu1
BL=/sys/class/backlight/backlight/bl_power
STATE=/sys/kernel/debug/dri/0/state

[ -x "$BIN" ] || { echo "missing $BIN"; exit 1; }

# 息屏状态下原厂 GUI 才能把 DRM CRTC 重新打开；先让用户点一下屏幕。
if [ -f "$BL" ] && [ "$(cat "$BL")" != "0" ]; then
    echo 0 > "$BL" 2>/dev/null || true
    sleep 1
fi
if [ -f "$BL" ] && [ "$(cat "$BL")" != "0" ]; then
    echo "屏幕正在息屏，请先点一下屏幕唤醒，再重新运行本脚本"
    exit 1
fi

# lmd 没跑就先拉起来（正常开机后它一直在）
if ! pgrep -x lmd >/dev/null 2>&1; then
    /etc/init.d/S90lmd start >/dev/null 2>&1 || true
    sleep 8
fi

# 换 GUI：杀掉原厂 gui，lmd 会自动用当前挂载的 /usr/bin/gui 重启
killall -9 gui 2>/dev/null || true
sleep 1
if grep -q " $GUI " /proc/mounts; then
    umount "$GUI" 2>/dev/null || umount -l "$GUI" 2>/dev/null || true
    sleep 1
fi
mount -o ro --bind "$BIN" "$GUI" || { echo "bind $BIN -> $GUI failed"; exit 1; }
sleep 1
killall -9 gui 2>/dev/null || true
sleep 2

# 检查显示控制器是否真的开着；关着的话现在只能靠重启恢复。
if [ -f "$STATE" ] && grep -A2 'crtc\[' "$STATE" | grep -q 'enable=0'; then
    echo "警告：DRM 显示控制器仍处于关闭状态，屏幕可能不亮。"
    echo "      请重启打印机，等屏幕亮起后再运行 start.sh。"
    exit 2
fi

echo "skreeu1 started as lmd GUI; lmd/unisrv/MQTT/Moonraker keep running"
