#!/bin/sh
# 事件框架 host 单元测试:clang(wasm32)+ node 执行
# 用法:./run_event_framework.sh
. "$(dirname "$0")/run_common.sh"
EXTRA_CFLAGS="-I ../../event_framework -I ../../lwrb/include/lwrb"
run_wasm_test event_framework \
    test_event_framework.c \
    ../../event_framework/event_framework.c \
    ../../lwrb/lwrb/lwrb.c
