/**
 * @file    cooling_actuator.c
 * @brief   冷却执行器实现(安全输出单写者:TIM8 PWM / ALARM_EN / PRESSURE_CTRL_EN)
 */

#include "cooling_actuator.h"

/* ========================== 单例状态 ========================== */

static TIM_HandleTypeDef *s_htim;

static GPIO_TypeDef *s_alarm_port;
static uint16_t      s_alarm_pin;
static GPIO_TypeDef *s_valve_port;
static uint16_t      s_valve_pin;

/* ========================== API 实现 ========================== */

HAL_StatusTypeDef CoolingActuator_Init(TIM_HandleTypeDef *htim,
                                       GPIO_TypeDef *alarm_port, uint16_t alarm_pin,
                                       GPIO_TypeDef *valve_port, uint16_t valve_pin)
{
    s_htim       = htim;
    s_alarm_port = alarm_port;
    s_alarm_pin  = alarm_pin;
    s_valve_port = valve_port;
    s_valve_pin  = valve_pin;

    /* 上电安全不变量:先归零 compare(MX 初始化 Pulse=2000 会产生 20% 启动
     * 毛刺),确认归零后再启动 PWM —— Init 返回即保证零输出运行。 */
    __HAL_TIM_SET_COMPARE(s_htim, TIM_CHANNEL_1, 0U);
    __HAL_TIM_SET_COMPARE(s_htim, TIM_CHANNEL_2, 0U);

    HAL_StatusTypeDef r1 = HAL_TIM_PWM_Start(s_htim, TIM_CHANNEL_1);
    HAL_StatusTypeDef r2 = HAL_TIM_PWM_Start(s_htim, TIM_CHANNEL_2);

    if (r1 == HAL_OK && r2 == HAL_OK) {
        return HAL_OK;
    }
    if (r1 == HAL_ERROR || r2 == HAL_ERROR) {
        return HAL_ERROR;
    }
    return HAL_BUSY;
}

void CoolingActuator_HeaterSetDuty(uint32_t duty_ch1, uint32_t duty_ch2)
{
    __HAL_TIM_SET_COMPARE(s_htim, TIM_CHANNEL_1, duty_ch1);
    __HAL_TIM_SET_COMPARE(s_htim, TIM_CHANNEL_2, duty_ch2);
}

void CoolingActuator_ValveSetEnable(uint8_t enable)
{
    HAL_GPIO_WritePin(s_valve_port, s_valve_pin,
                      enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void CoolingActuator_OverpressureLatch(void)
{
    /* ISR 安全:仅寄存器/GPIO 写,不触 HAL 状态机 */
    __HAL_TIM_SET_COMPARE(s_htim, TIM_CHANNEL_1, 0U);
    __HAL_TIM_SET_COMPARE(s_htim, TIM_CHANNEL_2, 0U);
    HAL_GPIO_WritePin(s_alarm_port, s_alarm_pin, GPIO_PIN_SET);
}

void CoolingActuator_OverpressureClear(void)
{
    HAL_GPIO_WritePin(s_alarm_port, s_alarm_pin, GPIO_PIN_RESET);
}
