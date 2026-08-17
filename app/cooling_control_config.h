/**
 * @file    cooling_control_config.h
 * @brief   冷却控制配置 —— 唯一权威源
 *
 * 规则:
 * - 加热片固定占空比、压力阈值只在本文件定义。
 * - 加热片为开环固定占空比(无 PID);超压时由 app 层关断(两通道 PWM 归零)。
 */

#ifndef COOLING_CONTROL_CONFIG_H
#define COOLING_CONTROL_CONFIG_H

#include "cooling_control.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ====================== 加热片固定占空比 ====================== */
/* 与 TIM8 ARR+1 尺度一致(当前 ARR=10000-1,即 10000 满量程)。
 * 2000 = 20% 占空比。两通道各自独立,便于单独整定。 */
#define COOLING_CTRL_HEATER_DUTY_CH1    2000.0f  /* 通道1(罐侧) */
#define COOLING_CTRL_HEATER_DUTY_CH2    2000.0f  /* 通道2(管侧) */

/* ====================== 压力阈值(PSI) ====================== */
#define COOLING_CTRL_TANK_CONNECTED_PSI 20.0f
#define COOLING_CTRL_ENABLE_PSI         20.0f
#define COOLING_CTRL_OVERPRESSURE_PSI   130.0f

/* ====================== 默认配置实例 ====================== */
static const CoolingCtrlConfig cooling_control_default_config = {
    .tank_connected_psi = COOLING_CTRL_TANK_CONNECTED_PSI,
    .ctrl_enable_psi    = COOLING_CTRL_ENABLE_PSI,
    .overpressure_psi   = COOLING_CTRL_OVERPRESSURE_PSI,
};

#ifdef __cplusplus
}
#endif

#endif /* COOLING_CONTROL_CONFIG_H */
