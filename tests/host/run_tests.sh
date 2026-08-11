#!/bin/sh
# 冷却控制模块 host 测试编译运行脚本
# 用法: ./run_tests.sh   (或 sh run_tests.sh)
# 需要任一 C 编译器:gcc / clang(可用 CC 环境变量指定)
set -e
cd "$(dirname "$0")"

CC="${CC:-gcc}"
BIN="/tmp/test_cooling_control"

echo "==> 编译器: $CC"
"$CC" -std=c99 -Wall -Wextra \
    -I ../../app -I ../../pid \
    test_cooling_control.c \
    ../../app/cooling_control.c \
    ../../pid/PID.c \
    -lm -o "$BIN"

echo "==> 运行:"
"$BIN"
