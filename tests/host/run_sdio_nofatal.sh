#!/bin/sh
# SD 无卡开机回归测试:clang(wasm32)+ node 执行
# 用法:
#   ./run_sdio_nofatal.sh        # 修复后代码 -> 期望 PASS
#   ./run_sdio_nofatal.sh void   # 修复前旧代码(需先 git stash 修复) -> 期望 FAIL(红验证)
. "$(dirname "$0")/run_common.sh"

VOID_FLAG=""
[ "${1:-}" = "void" ] && VOID_FLAG="-DSDIO_VOID_INIT"
EXTRA_CFLAGS="$VOID_FLAG"

run_wasm_test sdio_nofatal \
    test_sdio_nofatal.c \
    ../../Core/Src/sdio.c
