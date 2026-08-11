/**
 * @file    sim_test.c
 * @brief   冷却系统模拟测试(由 LVGL 定时器驱动,非阻塞)
 *
 * 行为:
 *   压力 ch1(ch2) 以三角波 0 -> 150 -> 0 循环(步长/相位不同)
 *   超压 >130 PSI 时自动显示/隐藏全屏警告
 *   其余状态(连接/液位/阀/加热)每 250ms 随机切换
 *
 * 依赖:SIM_TEST_ENABLE 宏控制(见 sim_test.h);主循环需循环调 lv_task_handler()
 */

#include "sim_test.h"

#if SIM_TEST_ENABLE

#include "cooling_ui.h"
#include "lvgl.h"
#include "main.h"       /* HAL_TIM_Base_Start_IT, tim 句柄 */
#include "cooling_control_config.h"  /* COOLING_CTRL_OVERPRESSURE_PSI */

/* 外部定时器句柄(供 LVGL tick 推进) */
extern TIM_HandleTypeDef htim13;

/* 压力三角波参数 */
#define SIM_PRESS_STEP_TANK   4.0f    /* 每 250ms 步长(ch1) */
#define SIM_PRESS_STEP_PIPE   3.0f    /* 每 250ms 步长(ch2,略有相位差) */
#define SIM_PRESS_MAX         150.0f
#define SIM_PRESS_MIN         0.0f

/* 简单 LCG 伪随机 */
static uint32_t s_rand = 0xdeadbeef;

static uint32_t sim_rand_u32(void)
{
    s_rand = s_rand * 1103515245U + 12345U;
    return s_rand;
}

/* 测试状态 */
static lv_timer_t *s_sim_timer;
static float  s_press_tank = 0.0f;
static float  s_press_pipe = 20.0f;  /* 初始偏移,使两通道相位不同 */
static int8_t s_dir_tank   = 1;
static int8_t s_dir_pipe   = 1;

static void sim_timer_cb(lv_timer_t * t)
{
    (void)t;

    /* ---- 1. 压力三角波 ---- */
    s_press_tank += s_dir_tank * SIM_PRESS_STEP_TANK;
    if (s_press_tank >= SIM_PRESS_MAX) { s_press_tank = SIM_PRESS_MAX; s_dir_tank = -1; }
    else if (s_press_tank <= SIM_PRESS_MIN) { s_press_tank = SIM_PRESS_MIN; s_dir_tank = 1; }

    s_press_pipe += s_dir_pipe * SIM_PRESS_STEP_PIPE;
    if (s_press_pipe >= SIM_PRESS_MAX) { s_press_pipe = SIM_PRESS_MAX; s_dir_pipe = -1; }
    else if (s_press_pipe <= SIM_PRESS_MIN) { s_press_pipe = SIM_PRESS_MIN; s_dir_pipe = 1; }

    cooling_ui_set_tank_pressure(s_press_tank);
    cooling_ui_set_pipe_pressure(s_press_pipe);

    /* ---- 2. 超压警告(随压力实时变化,同步 PB11 硬件报警) ---- */
    if (s_press_tank > COOLING_CTRL_OVERPRESSURE_PSI ||
        s_press_pipe > COOLING_CTRL_OVERPRESSURE_PSI) {
        cooling_ui_show_overpressure_warning();
        HAL_GPIO_WritePin(ALARM_EN_GPIO_Port, ALARM_EN_Pin, GPIO_PIN_SET);
    } else {
        cooling_ui_hide_overpressure_warning();
        HAL_GPIO_WritePin(ALARM_EN_GPIO_Port, ALARM_EN_Pin, GPIO_PIN_RESET);
    }

    /* ---- 3. PB10 压力控制器使能(管道压力 > 20 PSI) ---- */
    if (s_press_pipe > COOLING_CTRL_ENABLE_PSI) {
        HAL_GPIO_WritePin(PRESSURE_CTRL_EN_GPIO_Port, PRESSURE_CTRL_EN_Pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(PRESSURE_CTRL_EN_GPIO_Port, PRESSURE_CTRL_EN_Pin, GPIO_PIN_RESET);
    }

    /* ---- 4. 随机设备状态 ---- */
    cooling_ui_set_tank_connection((sim_rand_u32() & 1U) ? TANK_CONNECTED : TANK_DISCONNECTED);
    cooling_ui_set_coolant_level((coolant_level_t)(sim_rand_u32() % 3U));
    cooling_ui_set_valve_state((valve_state_t)((sim_rand_u32() % 2U) + (uint32_t)VALVE_CLOSED));
    cooling_ui_set_heater_state((heater_state_t)(sim_rand_u32() % 3U));
}

void App_Simulation_Init(void)
{
    /* 仅启动 TIM13 供 LVGL tick 推进(事件框架与 PWM/ADC 不启) */
    HAL_TIM_Base_Start_IT(&htim13);

    s_sim_timer = lv_timer_create(sim_timer_cb, 250, NULL);
}

#endif /* SIM_TEST_ENABLE */

/* 防止 empty translation unit 警告(宏关闭时) */
volatile int _sim_test_guard;
