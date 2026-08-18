/**
 * @file    fake/string.h
 * @brief   host 测试(wasm32 freestanding)最小 string 桩
 *
 * wasm32-unknown-unknown 无 libc;仅提供事件框架/lwrb 编译所需的
 * memset/memcpy 声明,实现由测试替身以 optnone 提供(防递归展开)。
 */
#ifndef FAKE_STRING_H
#define FAKE_STRING_H

#include <stddef.h>

void *memset(void *s, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);

#endif /* FAKE_STRING_H */
