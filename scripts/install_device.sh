#!/bin/bash
# 把编译好的 u1ui 安装（或更新）到打印机。
#
#   scripts/install_device.sh root@192.168.1.50            # 装二进制 + 字体 + 脚本
#   scripts/install_device.sh root@192.168.1.50 --no-fonts # 只更新二进制（字体已经在机器上时）
#   scripts/install_device.sh root@192.168.1.50 --start    # 装完直接接管屏幕
#
# 前提：本机能用 SSH 免密（或已配好密钥）登录打印机，且已经交叉编译出
# build-aarch64/u1ui（见 README）。
set -euo pipefail

TARGET="${1:-}"
if [ -z "$TARGET" ] || [ "$TARGET" = "-h" ]; then
    echo "用法: scripts/install_device.sh <user@host> [--no-fonts] [--start]" >&2
    exit 1
fi
shift

COPY_FONTS=1
START=0
for arg in "$@"; do
    case "$arg" in
        --no-fonts) COPY_FONTS=0 ;;
        --start)    START=1 ;;
        *) echo "未知参数: $arg" >&2; exit 1 ;;
    esac
done

cd "$(dirname "$0")/.."

BIN=build-aarch64/u1ui
[ -f "$BIN" ] || {
    echo "找不到 $BIN，先交叉编译：" >&2
    echo "  docker build -t u1ui-cross:1 -f docker/Dockerfile.cross ." >&2
    echo "  docker run --rm --user \$(id -u):\$(id -g) -e HOME=/tmp -v \$PWD:/src -w /src u1ui-cross:1" >&2
    exit 1
}

echo "==> 准备目录"
ssh "$TARGET" 'mkdir -p /userdata/u1ui/assets'

if [ "$COPY_FONTS" = 1 ]; then
    echo "==> 拷贝字体（约 34MB）"
    tar -C assets -czf - fonts | ssh "$TARGET" 'tar -C /userdata/u1ui/assets -xzf -'
fi

echo "==> 拷贝二进制和脚本"
scp -q "$BIN" "$TARGET:/userdata/u1ui/u1ui"
scp -q device/start.sh device/stop.sh "$TARGET:/userdata/u1ui/"
ssh "$TARGET" 'chmod +x /userdata/u1ui/u1ui /userdata/u1ui/start.sh /userdata/u1ui/stop.sh'

echo "==> 完成：/userdata/u1ui/u1ui ($(du -h "$BIN" | cut -f1))"

if [ "$START" = 1 ]; then
    echo "==> 接管屏幕"
    ssh "$TARGET" '/userdata/u1ui/start.sh'
else
    cat <<EOF

在打印机上执行：
  /userdata/u1ui/start.sh    # 接管屏幕（官方服务保持运行）
  /userdata/u1ui/stop.sh     # 恢复原厂 GUI
EOF
fi
