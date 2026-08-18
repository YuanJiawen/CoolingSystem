#!/bin/sh
# 共享测试装备(架构评审 2026-08-18,候选 5):工具链解析 + clang(wasm32) 编译运行胶水。
# 各 run_*.sh source 本文件后调用:run_wasm_test <套件名> <源文件...>
# node runner 命名约定:<套件名>_test.js。
#
# 环境变量(均可覆盖默认):
#   LLVM_BIN      LLVM bin 目录(默认 /c/Program Files/LLVM/bin)
#   CLANG         clang 可执行文件
#   NODE          node 可执行文件
#   EXTRA_CFLAGS  附加编译选项(如 -I ../../event_framework)
#   EXTRA_SRC_CFLAGS 同 EXTRA_CFLAGS(别名,取非空者)
set -e
cd "$(dirname "$0")"

LLVM_BIN="${LLVM_BIN:-/c/Program Files/LLVM/bin}"
TEST_CLANG="${CLANG:-$LLVM_BIN/clang.exe}"
TEST_NODE="${NODE:-node}"

# lld 是多目标链接器,按 argv[0] 选择 flavor;复制为 wasm-ld.exe 即启用 wasm 后端
# (副本放 /tmp,不污染源码树)
WASM_LD="/tmp/wasm-ld.exe"
[ -x "$WASM_LD" ] || cp "$LLVM_BIN/lld.exe" "$WASM_LD"

run_wasm_test() {
    _name="$1"; shift
    _out="/tmp/test_${_name}.wasm"
    _extra="${EXTRA_CFLAGS:-}"
    echo "==> 编译:"
    "$TEST_CLANG" --target=wasm32 -std=c99 -Wall -Wextra \
        -I fake -I ../../app -nostdlib $_extra \
        -fuse-ld="$WASM_LD" \
        -Wl,--no-entry \
        -Wl,--export=run_tests \
        -Wl,--export=g_last_fail_line \
        -Wl,--export-memory \
        -Wl,-zstack-size=65536 \
        "$@" -o "$_out"
    echo "==> 运行 (node):"
    "$TEST_NODE" "${_name}_test.js" "$_out"
}
