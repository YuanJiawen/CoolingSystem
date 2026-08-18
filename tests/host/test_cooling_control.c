/**
 * @file    test_cooling_control.c
 * @brief   冷却控制模块 host 单元测试(纯计算,零硬件依赖)
 *
 * 覆盖:
 *   1. ADC -> 压力(PSI)换算与负压截断
 *   2. 压力罐连接判定边界
 *   3. 超压判定边界(任一通道)
 *   4. 电磁阀使能边界
 *   5. 显示钳位边界(零点死区 + 量程上限)
 *
 * 构建/运行:sh run_cooling_control.sh(clang wasm + node)
 */

#include "cooling_control.h"
#include "cooling_control_config.h"

static int g_failures = 0;
int g_last_fail_line = 0;
#include "check.h"

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

    /* ---- 5. 显示钳位(死区 <1 归零;量程钳 0~150) ---- */
    CHECK_NEAR(CoolingControl_DisplayPressure(-5.0f), 0.0f, TOL, __LINE__);
    CHECK_NEAR(CoolingControl_DisplayPressure(0.99f), 0.0f, TOL, __LINE__);
    CHECK_NEAR(CoolingControl_DisplayPressure(1.0f), 1.0f, TOL, __LINE__);
    CHECK_NEAR(CoolingControl_DisplayPressure(75.5f), 75.5f, TOL, __LINE__);
    CHECK_NEAR(CoolingControl_DisplayPressure(150.0f), 150.0f, TOL, __LINE__);
    CHECK_NEAR(CoolingControl_DisplayPressure(206.7f), 150.0f, TOL, __LINE__); /* 满量程截断 */

    return g_failures;
}
