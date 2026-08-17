/**
 * @file    test_cooling_control.c
 * @brief   冷却控制模块 host 单元测试(纯计算,零硬件依赖)
 *
 * 覆盖:
 *   1. ADC -> 压力(PSI)换算与负压截断
 *   2. 压力罐连接判定边界
 *   3. 超压判定边界(任一通道)
 *   4. 电磁阀使能边界
 *
 * 构建/运行:sh run_cooling_control.sh(clang wasm + node)
 */

#include "cooling_control.h"
#include "cooling_control_config.h"

static int g_failures = 0;
int g_last_fail_line = 0;

#define CHECK(cond, line) \
    do { if (!(cond)) { g_failures++; g_last_fail_line = (line); } } while (0)

#define CHECK_NEAR(actual, expected, tol, line) \
    do { float _a = (actual), _e = (expected); \
         float _d = _a - _e; if (_d < 0.0f) _d = -_d; \
         if (!(_d <= (tol))) { g_failures++; g_last_fail_line = (line); } } while (0)

int run_tests(void)
{
    g_failures = 0;
    const CoolingCtrlConfig *cfg = &cooling_control_default_config;
    const float TOL = 1e-3f;

    /* ---- 1. ADC -> 压力(PSI)换算与负压截断 ---- */
    CHECK_NEAR(CoolingControl_PressureFromAdc(0), 0.0f, TOL, __LINE__);
    CHECK_NEAR(CoolingControl_PressureFromAdc(341), 0.0f, TOL, __LINE__);      /* ~0.5V -> 0 PSI */
    CHECK_NEAR(CoolingControl_PressureFromAdc(683), 18.824f, TOL, __LINE__);   /* ~1.0V -> ~18.8 PSI */
    CHECK_NEAR(CoolingControl_PressureFromAdc(4095), 206.767f, TOL, __LINE__); /* 满量程 6V */
    CHECK_NEAR(CoolingControl_PressureFromAdc(2701), 129.982f, TOL, __LINE__); /* 超压阈值附近 */

    /* ---- 2. 压力罐连接判定(>= 阈值) ---- */
    CHECK(CoolingControl_IsTankConnected(cfg, 20.0f) == 1, __LINE__);
    CHECK(CoolingControl_IsTankConnected(cfg, 19.99f) == 0, __LINE__);

    /* ---- 3. 超压判定(任一通道,严格大于) ---- */
    CHECK(CoolingControl_IsOverpressure(cfg, 130.0f, 0.0f) == 0, __LINE__);
    CHECK(CoolingControl_IsOverpressure(cfg, 130.01f, 0.0f) == 1, __LINE__);
    CHECK(CoolingControl_IsOverpressure(cfg, 0.0f, 130.01f) == 1, __LINE__);
    CHECK(CoolingControl_IsOverpressure(cfg, 0.0f, 0.0f) == 0, __LINE__);

    /* ---- 4. 电磁阀使能(管道压力,严格大于) ---- */
    CHECK(CoolingControl_ValveEnable(cfg, 20.01f) == 1, __LINE__);
    CHECK(CoolingControl_ValveEnable(cfg, 20.0f) == 0, __LINE__);
    CHECK(CoolingControl_ValveEnable(cfg, 0.0f) == 0, __LINE__);

    return g_failures;
}
