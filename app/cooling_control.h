/**
 * @file    cooling_control.h
 * @brief   冷却控制模块(纯计算,零硬件依赖)
 *
 * 设计要点(架构评审 2026-08-10,候选 1):
 * - 深模块:小 interface(设定值 + 压力值 -> 控制量),集中实现换算/PID/限幅/阈值判断。
 * - 纯计算:不 include HAL、LVGL、引脚宏;输入输出均为 float/状态,便于 host 测试。
 * - 单通道律:本模块不感知通道,由调用方(app 层)持有实例数组并循环驱动。
 * - 硬件访问(PWM/GPIO/ADC)位于本模块 seam 之后,由 app 层作为 adapter 完成。
 */

#ifndef COOLING_CONTROL_H
#define COOLING_CONTROL_H

#include <stdint.h>
#include "PID.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================== 配置结构 ========================== */

/**
 * @brief 冷却控制配置(唯一权威源,见 cooling_control_config.h)
 * @note  周期/限幅等数值必须与定时器实际配置一致,启动时由 app 层校验。
 */
typedef struct {
    /* PID 参数 */
    float kp;           /**< 比例增益 */
    float ki;           /**< 积分增益 */
    float kd;           /**< 微分增益 */
    float tau;          /**< 微分低通滤波时间常数 */
    float lim_min;      /**< 输出下限(对应 PWM 最小占空比) */
    float lim_max;      /**< 输出上限(对应 TIM8 ARR,即 10000) */
    float lim_min_int;  /**< 积分下限(抗饱和) */
    float lim_max_int;  /**< 积分上限(抗饱和) */
    float sample_time_s;/**< 控制周期(秒),必须与定时器中断周期一致 */
    /* 压力阈值(PSI) */
    float tank_connected_psi; /**< 压力罐连接判定阈值 */
    float ctrl_enable_psi;    /**< 电磁阀使能阈值(针对管道压力) */
    float overpressure_psi;   /**< 超压告警阈值 */
} CoolingCtrlConfig;

/* ========================== 输出结构 ========================== */

/**
 * @brief 单通道控制输出(状态快照的一部分)
 */
typedef struct {
    float    pwm;           /**< 限幅后的 PWM 输出值(0 .. lim_max) */
    uint8_t  tank_connected;/**< 1: 压力罐已连接(pressure >= 阈值) */
    uint8_t  overpressure;  /**< 1: 超压告警(pressure > 阈值) */
} CoolingCtrlOut;

/* ========================== 控制律状态 ========================== */

/**
 * @brief 单通道控制实例(内部持有 PID 状态,调用方不感知)
 */
typedef struct {
    PIDController pid;
} CoolingCtrl;

/* ========================== API ========================== */

/**
 * @brief 初始化单通道控制实例(按配置写入 PID 参数)
 */
void CoolingControl_Init(CoolingCtrl *ctrl, const CoolingCtrlConfig *cfg);

/**
 * @brief 单通道控制步进:输入设定值与当前压力,输出控制量
 * @note  纯计算,无副作用;pwm 已按 lim_min/lim_max 限幅。
 */
CoolingCtrlOut CoolingControl_Step(CoolingCtrl *ctrl, const CoolingCtrlConfig *cfg,
                                   float setpoint, float pressure);

/**
 * @brief ADC 原始值 -> 压力(PSI),含负压截断保护
 * @note  Vref=3V,12 位(4095);Vout = 0.5V + P*0.0266V
 */
float CoolingControl_PressureFromAdc(uint32_t adc_val);

/**
 * @brief 电磁阀使能判断(系统级,基于管道压力)
 */
uint8_t CoolingControl_ValveEnable(const CoolingCtrlConfig *cfg, float pipe_pressure);

/**
 * @brief 校验配置与定时器实际值是否一致(配置单源化的同步校验)
 * @param pwm_arr            PWM 定时器 ARR 计数值(如 TIM8 htim.Init.Period)
 * @param sample_period_ms   采样定时器中断周期(毫秒,如 TIM11)
 * @return 1 一致,0 不一致
 */
uint8_t CoolingControl_ValidateConfig(const CoolingCtrlConfig *cfg,
                                      uint32_t pwm_arr,
                                      uint32_t sample_period_ms);

#ifdef __cplusplus
}
#endif

#endif /* COOLING_CONTROL_H */
