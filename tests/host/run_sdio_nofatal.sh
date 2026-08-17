#!/bin/sh
# SD 无卡开机回归测试:clang(wasm32)+ node 执行
# 用法:
#   ./run_sdio_nofatal.sh        # 修复后代码 -> 期望 PASS
#   ./run_sdio_nofatal.sh void   # 修复前旧代码(需先 git stash 修复) -> 期望 FAIL(红验证)
#
# 本机无 gcc/Keil,采用 clang --target=wasm32 + lld(复制为 wasm-ld)+ node 的组合,
# 在不依赖 MSVC/Windows SDK 的情况下运行 freestanding C 测试。
set -e
cd "$(dirname "$0")"

LLVM_BIN="/c/Program Files/LLVM/bin"
CLANG="${CLANG:-$LLVM_BIN/clang.exe}"
NODE="${NODE:-node}"

# lld 是多目标链接器,按 argv[0] 选择 flavor;复制为 wasm-ld.exe 即启用 wasm 后端
# (副本放 /tmp,不污染源码树)
WASM_LD="/tmp/wasm-ld.exe"
[ -x "$WASM_LD" ] || cp "$LLVM_BIN/lld.exe" "$WASM_LD"

VOID_FLAG=""
[ "${1:-}" = "void" ] && VOID_FLAG="-DSDIO_VOID_INIT"

OUT="/tmp/test_sdio_nofatal.wasm"

echo "==> 编译 (${1:-fixed} variant):"
"$CLANG" --target=wasm32 -std=c99 -Wall -Wextra \
    -I fake -nostdlib $VOID_FLAG \
    -fuse-ld="$WASM_LD" \
    -Wl,--no-entry \
    -Wl,--export=run_tests \
    -Wl,--export=g_last_fail_line \
    -Wl,--export-memory \
    -Wl,-zstack-size=65536 \
    test_sdio_nofatal.c \
    ../../Core/Src/sdio.c \
    -o "$OUT"

echo "==> 运行 (node):"
"$NODE" sdio_nofatal_test.js "$OUT"
