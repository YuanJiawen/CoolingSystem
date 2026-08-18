#!/bin/sh
# 压力采样适配器 host 回归测试(链接真冷却执行器,验证 ISR→关断安全链):
# clang(wasm32)+ node 执行
# 用法:./run_pressure_sampler.sh
. "$(dirname "$0")/run_common.sh"
run_wasm_test pressure_sampler \
    test_pressure_sampler.c \
    ../../app/pressure_sampler.c \
    ../../app/cooling_actuator.c \
    ../../app/cooling_control.c
