/**
 * @file    fake/check.h
 * @brief   host 测试共享断言宏(架构评审 2026-08-18,候选 5:装备单源)
 *
 * 用法:测试文件先定义 `static int g_failures = 0;` 与 `int g_last_fail_line = 0;`,
 * 再包含本头文件。wasm 导出 g_last_fail_line 供 node runner 报告首个失败行号。
 */
#ifndef FAKE_CHECK_H
#define FAKE_CHECK_H

extern int g_last_fail_line;

#define CHECK(cond, line) \
    do { if (!(cond)) { g_failures++; g_last_fail_line = (line); } } while (0)

#define CHECK_NEAR(actual, expected, tol, line) \
    do { float _a = (actual), _e = (expected); \
         float _d = _a - _e; if (_d < 0.0f) _d = -_d; \
         if (!(_d <= (tol))) { g_failures++; g_last_fail_line = (line); } } while (0)

#endif /* FAKE_CHECK_H */
