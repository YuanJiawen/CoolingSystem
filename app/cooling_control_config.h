/**
 * @file    cooling_control_config.h
 * @brief   冷却控制配置 —— 唯一权威源(候选 2 单源化)
 *
 * 规则:
 * - 控制周期、PWM 尺度、PID 参数、压力阈值只在本文件定义。
 * - 定时器(CubeMX 生成)必须与之匹配,启动时由 app 层调用
 *   CoolingControl_ValidateConfig() 校验,不一致则显式报错。
 */

#ifndef COOLING_CONTROL_CONFIG_H
#define COOLING_CONTROL_CONFIG_H

#include "cooling_control.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ====================== 控制周期 ====================== */
/* 必须与 TIM11 中断周期一致(当前 500ms) */
#define COOLING_CTRL_SAMPLE_TIME_MS    500
#define COOLING_CTRL_SAMPLE_TIME_S     ((COOLING_CTRL_SAMPLE_TIME_MS) / 1000.0f)

/* ====================== PWM 输出尺度 ====================== */
/* 必须与 TIM8 ARR+1 一致(当前 ARR = 10000-1) */
#define COOLING_CTRL_PWM_LIM_MAX       10000.0f
#define COOLING_CTRL_PWM_LIM_MIN       0.0f

/* ====================== PID 参数 ====================== */
/* 【需用户整定】 */
#define COOLING_CTRL_PID_KP            1.0f
#define COOLING_CTRL_PID_KI            0.5f
#define COOLING_CTRL_PID_KD            0.0f
#define COOLING_CTRL_PID_TAU           0.02f
#define COOLING_CTRL_PID_LIM_MIN_INT   (-5000.0f)
#define COOLING_CTRL_PID_LIM_MAX_INT   (5000.0f)

/* ====================== 压力阈值(PSI) ====================== */
#define COOLING_CTRL_TANK_CONNECTED_PSI 20.0f
#define COOLING_CTRL_ENABLE_PSI         20.0f
#define COOLING_CTRL_OVERPRESSURE_PSI   130.0f

/* ====================== 通道数 ====================== */
#define COOLING_CTRL_CHANNELS           2

/* ====================== 默认配置实例 ====================== */
/* 两通道共用同一份参数;如需独立整定,改为 CoolingCtrlConfig 数组 */
static const CoolingCtrlConfig cooling_control_default_config = {
    .kp               = COOLING_CTRL_PID_KP,
    .ki               = COOLING_CTRL_PID_KI,
    .kd               = COOLING_CTRL_PID_KD,
    .tau              = COOLING_CTRL_PID_TAU,
    .lim_min          = COOLING_CTRL_PWM_LIM_MIN,
    .lim_max          = COOLING_CTRL_PWM_LIM_MAX,
    .lim_min_int      = COOLING_CTRL_PID_LIM_MIN_INT,
    .lim_max_int      = COOLING_CTRL_PID_LIM_MAX_INT,
    .sample_time_s    = COOLING_CTRL_SAMPLE_TIME_S,
    .tank_connected_psi = COOLING_CTRL_TANK_CONNECTED_PSI,
    .ctrl_enable_psi  = COOLING_CTRL_ENABLE_PSI,
    .overpressure_psi = COOLING_CTRL_OVERPRESSURE_PSI,
};

#ifdef __cplusplus
}
#endif

#endif /* COOLING_CONTROL_CONFIG_H */
