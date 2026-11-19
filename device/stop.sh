#!/bin/sh
# 停掉 skreeu1，恢复原厂屏幕 GUI（lmd / unisrv 一直保持运行）。
#
# 用法（在打印机上）：/userdata/skreeu1/stop.sh
set -e

GUI=/usr/bin/gui
CFG=/oem/printer_data/config/extended/extended2.cfg

killall -9 gui lmd unisrv 2>/dev/null || true
sleep 2
if grep -q " $GUI " /proc/mounts; then
    umount "$GUI" 2>/dev/null || umount -l "$GUI" 2>/dev/null || true
    sleep 1
fi

# 默认恢复原厂 GUI；想回 HelixScreen 就把 snapmaker 改成 helixscreen
if [ -x /usr/local/bin/extended-config.py ] && [ -f "$CFG" ]; then
    /usr/local/bin/extended-config.py add "$CFG" components gui snapmaker \
        >/dev/null 2>&1 || true
fi

/etc/init.d/S90lmd start >/dev/null 2>&1 || true
sleep 2
echo "stock gui restored (lmd/unisrv running)"
