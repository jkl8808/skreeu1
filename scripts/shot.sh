#!/bin/bash
# 生成单页截图：shot.sh <page> <out.png> [host] [port]
set -e
PAGE="${1:-0}"
OUT="${2:-/shots/page.png}"
HOST="${3:-${SKREEU1_HOST:-127.0.0.1}}"
PORT="${4:-7125}"
exec ./build/skreeu1 --shot "$OUT" --page "$PAGE" --host "$HOST" --port "$PORT"
