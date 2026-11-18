#!/bin/bash
# 从打印机上取交叉链接需要的 .so（固件自带的 freetype / png / curl）。
# 这些库有版权，所以不放进仓库，部署前自己拷一份。
#
#   scripts/fetch_sysroot.sh root@192.168.1.50
#
# 产物：sysroot/lib/libfreetype.so.6、libpng16.so.16、libcurl.so.4（及实际文件与软链）
set -euo pipefail

TARGET="${1:-}"
if [ -z "$TARGET" ]; then
    echo "用法: scripts/fetch_sysroot.sh <user@host>" >&2
    exit 1
fi

cd "$(dirname "$0")/.."
mkdir -p sysroot/lib

for soname in libfreetype.so.6 libpng16.so.16 libcurl.so.4; do
    real=$(ssh "$TARGET" "readlink -f /usr/lib/$soname")
    if [ -z "$real" ]; then
        echo "打印机上找不到 $soname" >&2
        exit 1
    fi
    base=$(basename "$real")
    echo "==> $soname -> $base"
    scp -q "$TARGET:$real" "sysroot/lib/$base"
    ln -sf "$base" "sysroot/lib/$soname"
done

echo "==> sysroot/lib 已就绪"
ls -l sysroot/lib
