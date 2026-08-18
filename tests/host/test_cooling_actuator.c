/**
 * @file    test_cooling_actuator.c
 * @brief   冷却执行器 cooling_actuator 的 host 单元测试(fake TIM/GPIO 仪器化)
 *
 * 覆盖:
 *   1. Init 上电安全顺序:两通道 compare 先归零,之后才启动 PWM(无启动毛刺)
 *   2. Init 状态映射:两通道 OK → HAL_OK;任一 HAL_ERROR → HAL_ERROR
 *   3. HeaterSetDuty 按通道写 compare
 *   4. ValveSetEnable 置/清阀 GPIO
 *   5. OverpressureLatch:两通道立即归零 + ALARM 置位
 *   6. OverpressureClear:仅清 ALARM,不动 compare
 *
 * 构建/运行:sh run_cooling_actuator.sh(clang wasm + node)
 */

#include "cooling_actuator.h"

int g_last_fail_line = 0;

/* ==================== 测试替身(记录 TIM/GPIO 调用) ==================== */

static TIM_HandleTypeDef g_fake_htim;

/* 用不同假地址区分告警/阀引脚 */
#define FAKE_ALARM_PORT ((GPIO_TypeDef *)0x11)
#define FAKE_VALVE_PORT ((GPIO_TypeDef *)0x22)
#define FAKE_ALARM_PIN  0x0001U
#define FAKE_VALVE_PIN  0x0002U

/* 操作日志:1=compare_ch1 2=compare_ch2 3=start_ch1 4=start_ch2 */
static int g_op_log[64];
static int g_op_count;
static uint32_t g_compare[2];        /* 各通道最近一次 compare 值 */
static uint32_t g_gpio_alarm;        /* 0=RESET 1=SET(按告警引脚过滤) */
static uint32_t g_gpio_valve;
static int g_pwm_start_rc = 0;       /* HAL_TIM_PWM_Start 返回值(0=HAL_OK) */

void fake_tim_set_compare(TIM_HandleTypeDef *htim, uint32_t channel, uint32_t value)
{
    (void)htim;
    if (g_op_count < 64) { g_op_log[g_op_count++] = (channel == TIM_CHANNEL_1) ? 1 : 2; }
    g_compare[channel] = value;
}

HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *htim, uint32_t Channel)
{
    (void)htim;
    if (g_op_count < 64) { g_op_log[g_op_count++] = (Channel == TIM_CHANNEL_1) ? 3 : 4; }
    return g_pwm_start_rc;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState)
{
    (void)GPIO_Pin;
    uint32_t v = (PinState == GPIO_PIN_SET) ? 1U : 0U;
    if (GPIOx == FAKE_ALARM_PORT) { g_gpio_alarm = v; }
    if (GPIOx == FAKE_VALVE_PORT) { g_gpio_valve = v; }
}

/* ==================== 测试主体 ==================== */

static int g_failures = 0;
#include "check.h"

static void reset_observers(void)
{
    g_op_count = 0;
    g_compare[0] = 0xFFFFU;
    g_compare[1] = 0xFFFFU;
    g_gpio_alarm = 0xFFFFU;
    g_gpio_valve = 0xFFFFU;
    g_pwm_start_rc = HAL_OK;
}

int run_tests(void)
{
    g_failures = 0;

    /* ---- 场景 1:Init 上电安全顺序(compare 先归零,PWM 后启动) ---- */
    reset_observers();
    HAL_StatusTypeDef rc = CoolingActuator_Init(&g_fake_htim,
                                                FAKE_ALARM_PORT, FAKE_ALARM_PIN,
                                                FAKE_VALVE_PORT, FAKE_VALVE_PIN);
    CHECK(rc == HAL_OK, __LINE__);
    CHECK(g_op_count == 4, __LINE__);
    CHECK(g_op_log[0] == 1 && g_op_log[1] == 2, __LINE__);  /* 先两通道 compare */
    CHECK(g_compare[0] == 0 && g_compare[1] == 0, __LINE__); /* 且值为 0 */
    CHECK(g_op_log[2] == 3 && g_op_log[3] == 4, __LINE__);  /* 后启动两通道 */

    /* ---- 场景 2:Init 状态映射(任一 ERROR → ERROR) ---- */
    reset_observers();
    g_pwm_start_rc = HAL_ERROR;
    rc = CoolingActuator_Init(&g_fake_htim,
                              FAKE_ALARM_PORT, FAKE_ALARM_PIN,
                              FAKE_VALVE_PORT, FAKE_VALVE_PIN);
    CHECK(rc == HAL_ERROR, __LINE__);
    g_pwm_start_rc = HAL_BUSY;
    rc = CoolingActuator_Init(&g_fake_htim,
                              FAKE_ALARM_PORT, FAKE_ALARM_PIN,
                              FAKE_VALVE_PORT, FAKE_VALVE_PIN);
    CHECK(rc == HAL_BUSY, __LINE__);

    /* ---- 场景 3:HeaterSetDuty 按通道写 compare ---- */
    reset_observers();
    CoolingActuator_Init(&g_fake_htim,
                         FAKE_ALARM_PORT, FAKE_ALARM_PIN,
                         FAKE_VALVE_PORT, FAKE_VALVE_PIN);
    CoolingActuator_HeaterSetDuty(2000U, 3000U);
    CHECK(g_compare[0] == 2000U, __LINE__);
    CHECK(g_compare[1] == 3000U, __LINE__);

    /* ---- 场景 4:ValveSetEnable 置/清阀 GPIO(不碰告警) ---- */
    reset_observers();
    CoolingActuator_Init(&g_fake_htim,
                         FAKE_ALARM_PORT, FAKE_ALARM_PIN,
                         FAKE_VALVE_PORT, FAKE_VALVE_PIN);
    CoolingActuator_ValveSetEnable(1U);
    CHECK(g_gpio_valve == 1U, __LINE__);
    CHECK(g_gpio_alarm == 0xFFFFU, __LINE__); /* 未写过告警 */
    CoolingActuator_ValveSetEnable(0U);
    CHECK(g_gpio_valve == 0U, __LINE__);

    /* ---- 场景 5:OverpressureLatch = 加热归零 + ALARM 置位 ---- */
    reset_observers();
    CoolingActuator_Init(&g_fake_htim,
                         FAKE_ALARM_PORT, FAKE_ALARM_PIN,
                         FAKE_VALVE_PORT, FAKE_VALVE_PIN);
    CoolingActuator_HeaterSetDuty(2000U, 2000U);
    CoolingActuator_OverpressureLatch();
    CHECK(g_compare[0] == 0U, __LINE__);
    CHECK(g_compare[1] == 0U, __LINE__);
    CHECK(g_gpio_alarm == 1U, __LINE__);
    CHECK(g_gpio_valve == 0xFFFFU, __LINE__); /* 锁存不动阀 */

    /* ---- 场景 6:OverpressureClear 仅清 ALARM ---- */
    reset_observers();
    CoolingActuator_Init(&g_fake_htim,
                         FAKE_ALARM_PORT, FAKE_ALARM_PIN,
                         FAKE_VALVE_PORT, FAKE_VALVE_PIN);
    CoolingActuator_OverpressureLatch();
    CoolingActuator_HeaterSetDuty(2000U, 2000U); /* 恢复属控制策略 */
    CoolingActuator_OverpressureClear();
    CHECK(g_gpio_alarm == 0U, __LINE__);
    CHECK(g_compare[0] == 2000U, __LINE__); /* compare 不被触碰 */
    CHECK(g_compare[1] == 2000U, __LINE__);

    return g_failures;
}
