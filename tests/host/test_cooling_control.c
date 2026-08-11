/**
 * @file    test_cooling_control.c
 * @brief   冷却控制模块 host 单元测试(纯计算,零硬件依赖)
 *
 * 编译运行(任一有 C 编译器的机器):
 *   gcc -std=c99 -Wall -Wextra -I ../../app -I ../../pid \
 *       test_cooling_control.c ../../app/cooling_control.c ../../pid/PID.c \
 *       -lm -o test_cooling_control && ./test_cooling_control
 *   (或直接运行同级 run_tests.sh)
 *
 * 退出码:0 = 全部通过;非 0 = 有失败断言。
 */

#include <stdio.h>
#include <math.h>

#include "cooling_control.h"
#include "cooling_control_config.h"

static int g_failures = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (cond) {                                                            \
            printf("  [PASS] %s\n", msg);                                      \
        } else {                                                               \
            printf("  [FAIL] %s  (%s:%d)\n", msg, __FILE__, __LINE__);         \
            g_failures++;                                                      \
        }                                                                      \
    } while (0)

#define CHECK_NEAR(actual, expected, tol, msg)                                 \
    do {                                                                       \
        float _a = (actual), _e = (expected);                                  \
        if (fabsf(_a - _e) <= (tol)) {                                         \
            printf("  [PASS] %s (%.6f)\n", msg, (double)_a);                   \
        } else {                                                               \
            printf("  [FAIL] %s: got %.6f expected %.6f (tol %.6f)\n",         \
                   msg, (double)_a, (double)_e, (double)(tol));                \
            g_failures++;                                                      \
        }                                                                      \
    } while (0)

static const float TOL = 1e-3f;

/* 测试 1:ADC -> 压力(PSI)换算与负压截断 */
static void test_pressure_from_adc(void)
{
    printf("[1] CoolingControl_PressureFromAdc\n");

    /* 公式: voltage = (adc*2/4095)*3 ; pressure = (voltage-0.5)/0.0266 ; 负值截断为 0
     * 注: 该公式满量程(adc=4095)对应 6.0V -> 206.77 PSI,与原 App_Calculate_Pressure 一致,
     *     与注释"Vref=3V"不符,系硬件标定约定,勿擅自改动。 */
    CHECK_NEAR(CoolingControl_PressureFromAdc(0), 0.0f, TOL, "adc=0 -> 负压截断为 0");
    CHECK_NEAR(CoolingControl_PressureFromAdc(341), 0.0f, TOL, "adc=341 (~0.5V) -> 0 PSI");
    CHECK_NEAR(CoolingControl_PressureFromAdc(683), 18.824f, TOL, "adc=683 (~1.0V) -> ~18.8 PSI");
    CHECK_NEAR(CoolingControl_PressureFromAdc(4095), 206.767f, TOL, "adc=4095 (6.0V 满量程)");
    CHECK_NEAR(CoolingControl_PressureFromAdc(2701), 129.982f, TOL, "adc=2701 -> 超压阈值附近 ~130 PSI");
}

/* 测试 2:单通道控制律首拍数值(PID 递推手算) */
static void test_step_first_cycle(void)
{
    CoolingCtrl ctrl;
    CoolingCtrlConfig cfg = cooling_control_default_config;
    CoolingCtrlOut out;

    printf("[2] CoolingControl_Step 首拍\n");

    CoolingControl_Init(&ctrl, &cfg);

    /* setpoint=50, pressure=0, 默认配置(Kp=1 Ki=0.5 Kd=0 T=0.5)
     * error=50, P=50, integrator += 0.5*0.5*0.5*(50+0)=6.25, diff=0
     * out = 56.25 */
    out = CoolingControl_Step(&ctrl, &cfg, 50.0f, 0.0f);
    CHECK_NEAR(out.pwm, 56.25f, TOL, "首拍 PWM = 56.25");
    CHECK(out.tank_connected == 0, "0 PSI 未连接");
    CHECK(out.overpressure == 0, "0 PSI 无超压");

    /* 第二拍: integrator += 0.5*0.5*0.5*(50+50)=12.5 -> 18.75, out=68.75 */
    out = CoolingControl_Step(&ctrl, &cfg, 50.0f, 0.0f);
    CHECK_NEAR(out.pwm, 68.75f, TOL, "第二拍 PWM = 68.75");
}

/* 测试 3:输出限幅与积分抗饱和 */
static void test_limits(void)
{
    CoolingCtrl ctrl;
    CoolingCtrlConfig cfg = cooling_control_default_config;
    CoolingCtrlOut out;

    printf("[3] 限幅与抗饱和\n");

    CoolingControl_Init(&ctrl, &cfg);

    /* 大误差:setpoint=20000 -> P=20000 -> 输出被 lim_max=10000 截断 */
    out = CoolingControl_Step(&ctrl, &cfg, 20000.0f, 0.0f);
    CHECK_NEAR(out.pwm, COOLING_CTRL_PWM_LIM_MAX, TOL, "输出上限截断 10000");

    /* 持续大误差多拍:积分器封顶 limMaxInt=5000 */
    for (int i = 0; i < 500; i++) {
        out = CoolingControl_Step(&ctrl, &cfg, 20000.0f, 0.0f);
    }
    CHECK_NEAR(ctrl.pid.integrator, COOLING_CTRL_PID_LIM_MAX_INT, TOL, "积分器封顶 5000");

    /* 输出始终不越界 */
    CHECK(out.pwm <= COOLING_CTRL_PWM_LIM_MAX, "输出永不上限越界");
    CHECK(out.pwm >= COOLING_CTRL_PWM_LIM_MIN, "输出永不下限越界");
}

/* 测试 4:阈值判定边界 */
static void test_thresholds(void)
{
    CoolingCtrl ctrl;
    CoolingCtrlConfig cfg = cooling_control_default_config;
    CoolingCtrlOut out;

    printf("[4] 连接/超压阈值边界\n");

    CoolingControl_Init(&ctrl, &cfg);

    out = CoolingControl_Step(&ctrl, &cfg, 50.0f, 20.0f);
    CHECK(out.tank_connected == 1, "pressure=20 -> 已连接 (>=20)");
    out = CoolingControl_Step(&ctrl, &cfg, 50.0f, 19.99f);
    CHECK(out.tank_connected == 0, "pressure=19.99 -> 未连接");
    out = CoolingControl_Step(&ctrl, &cfg, 50.0f, 130.01f);
    CHECK(out.overpressure == 1, "pressure=130.01 -> 超压 (>130)");
    out = CoolingControl_Step(&ctrl, &cfg, 50.0f, 130.0f);
    CHECK(out.overpressure == 0, "pressure=130.0 -> 未超压 (严格大于)");
}

/* 测试 5:电磁阀使能 */
static void test_valve_enable(void)
{
    CoolingCtrlConfig cfg = cooling_control_default_config;

    printf("[5] CoolingControl_ValveEnable\n");

    CHECK(CoolingControl_ValveEnable(&cfg, 20.01f) == 1, "管道压力 20.01 -> 使能");
    CHECK(CoolingControl_ValveEnable(&cfg, 20.0f) == 0, "管道压力 20.0 -> 不使能 (严格大于)");
    CHECK(CoolingControl_ValveEnable(&cfg, 0.0f) == 0, "管道压力 0 -> 不使能");
}

/* 测试 6:配置校验(候选 2 单源化) */
static void test_validate_config(void)
{
    CoolingCtrlConfig cfg = cooling_control_default_config;

    printf("[6] CoolingControl_ValidateConfig\n");

    /* 一致: lim_max=10000 == arr+1(9999+1), T=0.5s == 500ms */
    CHECK(CoolingControl_ValidateConfig(&cfg, 9999u, 500u) == 1, "ARR=9999/500ms 一致");

    /* 不一致 */
    CHECK(CoolingControl_ValidateConfig(&cfg, 9000u, 500u) == 0, "ARR 不符 -> 校验失败");
    CHECK(CoolingControl_ValidateConfig(&cfg, 9999u, 250u) == 0, "周期不符 -> 校验失败");
}

/* 测试 7:多实例独立性(两个通道互不干扰) */
static void test_independent_channels(void)
{
    CoolingCtrl a, b;
    CoolingCtrlConfig cfg = cooling_control_default_config;
    CoolingCtrlOut oa, ob;

    printf("[7] 通道实例独立性\n");

    CoolingControl_Init(&a, &cfg);
    CoolingControl_Init(&b, &cfg);

    oa = CoolingControl_Step(&a, &cfg, 50.0f, 0.0f);   /* a: 误差 50 -> 56.25 */
    ob = CoolingControl_Step(&b, &cfg, 50.0f, 30.0f);  /* b: 误差 20 -> P=20, integ=2.5 -> 22.5 */

    CHECK_NEAR(oa.pwm, 56.25f, TOL, "通道 a 首拍 56.25");
    CHECK_NEAR(ob.pwm, 22.5f, TOL, "通道 b 首拍 22.5");
}

int main(void)
{
    printf("cooling_control host tests (T=%ds, Kp=%.1f, Ki=%.1f, Kd=%.1f)\n",
           (int)COOLING_CTRL_SAMPLE_TIME_S, (double)COOLING_CTRL_PID_KP,
           (double)COOLING_CTRL_PID_KI, (double)COOLING_CTRL_PID_KD);

    test_pressure_from_adc();
    test_step_first_cycle();
    test_limits();
    test_thresholds();
    test_valve_enable();
    test_validate_config();
    test_independent_channels();

    printf("\n结果: %s (%d 失败)\n", g_failures ? "FAILED" : "ALL PASSED", g_failures);
    return g_failures ? 1 : 0;
}
