#!/bin/sh
# 停掉 skreeu1，恢复原厂屏幕 GUI。
#
# 只杀掉 gui 子进程，不动 lmd / unisrv —— 和 start.sh 对称。
#
# 用法（在打印机上）：/userdata/skreeu1/stop.sh
set -e

GUI=/usr/bin/gui

killall -9 gui 2>/dev/null || true
sleep 1
if grep -q " $GUI " /proc/mounts; then
    umount "$GUI" 2>/dev/null || umount -l "$GUI" 2>/dev/null || true
    sleep 1
fi
# 让 lmd 用原厂 /usr/bin/gui 重新拉起
killall -9 gui 2>/dev/null || true
sleep 2
echo "stock gui restored (lmd/unisrv keep running)"
