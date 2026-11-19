#!/bin/bash
# 交叉编译给 Snapmaker U1（aarch64，/dev/fb0 + evdev）
# 在 skreeu1-cross 容器里运行；宿主机也可以直接执行（需要 aarch64-linux-gnu-* 工具链）。
set -e

cd "$(dirname "$0")/.."

BUILD_DIR="${BUILD_DIR:-build-aarch64}"

cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake \
    -DSKREEU1_DEVICE=ON \
    -DCMAKE_BUILD_TYPE=Release

cmake --build "$BUILD_DIR" -j"$(nproc)"

if command -v aarch64-linux-gnu-strip >/dev/null; then
    aarch64-linux-gnu-strip "$BUILD_DIR/skreeu1"
fi

file "$BUILD_DIR/skreeu1"
ls -lh "$BUILD_DIR/skreeu1"
