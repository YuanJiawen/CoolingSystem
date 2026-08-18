#!/bin/sh
# 冷却执行器 host 单元测试:clang(wasm32)+ node 执行
# 用法:./run_cooling_actuator.sh
. "$(dirname "$0")/run_common.sh"
run_wasm_test cooling_actuator \
    test_cooling_actuator.c \
    ../../app/cooling_actuator.c
