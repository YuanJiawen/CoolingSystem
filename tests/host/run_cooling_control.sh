#!/bin/sh
# 冷却控制模块 host 回归测试:clang(wasm32)+ node 执行
# 用法:./run_cooling_control.sh
. "$(dirname "$0")/run_common.sh"
run_wasm_test cooling_control \
    test_cooling_control.c \
    ../../app/cooling_control.c
