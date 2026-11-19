#!/bin/sh
# 让 skreeu1 接管屏幕，同时保留 lmd / unisrv / MQTT / Moonraker 等官方服务。
#
# 原理：lmd 固定 fork /usr/bin/gui。先让 lmd 正常启动（原厂 GUI），
# 再把 skreeu1 只读 bind 到 /usr/bin/gui，杀掉当前 gui 子进程，lmd 会用 skreeu1 重启它。
# 这样官方 App / 切片软件依赖的 unisrv、3893 MQTT、7125 Moonraker 都还在。
#
# 用法（在打印机上）：
#   /userdata/skreeu1/start.sh
#   SKREEU1_READONLY=1 /userdata/skreeu1/start.sh   # 只读模式（环境变量会被 lmd 继承）
#
# 恢复原厂界面：/userdata/skreeu1/stop.sh
set -e

GUI=/usr/bin/gui
BIN=/userdata/skreeu1/skreeu1
CFG=/oem/printer_data/config/extended/extended2.cfg

[ -x "$BIN" ] || { echo "missing $BIN"; exit 1; }

# 1) 清掉旧实例。S90lmd stop 对 lmd 不一定生效，直接 kill。
killall -9 gui lmd unisrv 2>/dev/null || true
sleep 2
if grep -q " $GUI " /proc/mounts; then
    umount "$GUI" 2>/dev/null || umount -l "$GUI" 2>/dev/null || true
    sleep 1
fi

# 2) 不让 HelixScreen 的 hook 把屏幕抢回去（旧固件没有这个脚本，跳过）
if [ -x /usr/local/bin/extended-config.py ] && [ -f "$CFG" ]; then
    /usr/local/bin/extended-config.py add "$CFG" components gui snapmaker \
        >/dev/null 2>&1 || true
fi

# 3) 正常启动 lmd（此时是原厂 GUI，官方服务全部起来）
/etc/init.d/S90lmd start >/dev/null 2>&1 || true
sleep 8

# 4) 换上 skreeu1，让 lmd 重启 GUI 子进程
mount -o ro --bind "$BIN" "$GUI" || { echo "bind $BIN -> $GUI failed"; exit 1; }
killall gui 2>/dev/null || true
sleep 2
echo "skreeu1 started as lmd GUI; official services (unisrv/MQTT/Moonraker) are running"
