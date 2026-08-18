/**
 * @file    cooling_control.h
 * @brief   冷却控制模块(纯计算,零硬件依赖)
 *
 * 设计要点(架构评审 2026-08-10 / 2026-08-17):
 * - 深模块:集中实现压力换算、阈值判定(罐连接/阀使能/超压)。
 * - 纯计算:不 include HAL、LVGL、引脚宏;输入输出均为 float/状态,便于 host 测试。
 * - 加热片为开环固定占空比(见 cooling_control_config.h),超压时由调用方关断;
 *   本模块只提供超压判定,不持有 PID/占空比状态。
 * - 硬件访问(PWM/GPIO/ADC)位于本模块 seam 之后,由 app 层作为 adapter 完成。
 */
#ifndef COOLING_CONTROL_H
#define COOLING_CONTROL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 冷却控制配置(唯一权威源,见 cooling_control_config.h)
 */
typedef struct {
    float tank_connected_psi; /**< 压力罐连接判定阈值(PSI) */
    float ctrl_enable_psi;    /**< 电磁阀使能阈值(管道压力,PSI) */
    float overpressure_psi;   /**< 超压关断阈值(PSI) */
} CoolingCtrlConfig;

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
 * @brief 压力罐连接判定(pressure >= 阈值)
 */
uint8_t CoolingControl_IsTankConnected(const CoolingCtrlConfig *cfg, float pressure);

/**
 * @brief 超压判定(任一通道 pressure > 阈值)
 * @note  调用方据此关断加热片(两通道 PWM 归零)。
 */
uint8_t CoolingControl_IsOverpressure(const CoolingCtrlConfig *cfg,
                                      float pressure_1, float pressure_2);

/**
 * @brief 显示钳位(纯计算,单源):<1 PSI 零点死区归零,并钳位到 0~150 PSI
 * @note  仅供显示路径消费;控制判定一律使用原始压力,行为不变。
 */
float CoolingControl_DisplayPressure(float psi);

#ifdef __cplusplus
}
#endif

#endif /* COOLING_CONTROL_H */
