#!/bin/sh
# 压力采样适配器 host 回归测试:clang(wasm32)+ node 执行
# 用法:./run_pressure_sampler.sh
#
# 本机无 gcc/Keil,采用 clang --target=wasm32 + lld(复制为 wasm-ld)+ node 组合,
# 在不依赖 MSVC/Windows SDK 的情况下运行 freestanding C 测试。
set -e
cd "$(dirname "$0")"

LLVM_BIN="/c/Program Files/LLVM/bin"
CLANG="${CLANG:-$LLVM_BIN/clang.exe}"
NODE="${NODE:-node}"

# lld 是多目标链接器,按 argv[0] 选择 flavor;复制为 wasm-ld.exe 即启用 wasm 后端
WASM_LD="/tmp/wasm-ld.exe"
[ -x "$WASM_LD" ] || cp "$LLVM_BIN/lld.exe" "$WASM_LD"

OUT="/tmp/test_pressure_sampler.wasm"

echo "==> 编译:"
"$CLANG" --target=wasm32 -std=c99 -Wall -Wextra \
    -I fake -I ../../app -nostdlib \
    -fuse-ld="$WASM_LD" \
    -Wl,--no-entry \
    -Wl,--export=run_tests \
    -Wl,--export=g_last_fail_line \
    -Wl,--export-memory \
    -Wl,-zstack-size=65536 \
    test_pressure_sampler.c \
    ../../app/pressure_sampler.c \
    ../../app/cooling_control.c \
    -o "$OUT"

echo "==> 运行 (node):"
"$NODE" pressure_sampler_test.js "$OUT"
