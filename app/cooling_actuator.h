/**
 * @file    cooling_actuator.h
 * @brief   冷却执行器 —— 安全输出深模块(加热片 PWM / 电磁阀 / 超压告警 GPIO)
 *
 * 设计要点(架构评审 2026-08-18,候选 1):
 * - 深模块:所有安全写出收进一个小接口;「超压 → 加热片归零 + 告警置位」
 *   的映射知识住进本模块,调用者(采样器 ISR / 控制事件 / 仿真)不自行拼装。
 * - 单写者:全仓对 ALARM_EN / PRESSURE_CTRL_EN / TIM8 CCR 的写仅经本模块,
 *   仿真模式与工作模式共享同一接口(接缝因此成立)。
 * - 上电安全为模块不变量:Init 先归零 compare 再启动 PWM(消灭启动毛刺)。
 * - 超压快路径 ISR 安全:OverpressureLatch 只写寄存器/GPIO,可在中断内调用。
 */
#ifndef COOLING_ACTUATOR_H
#define COOLING_ACTUATOR_H

#include <stdint.h>
#include "stm32f4xx_hal.h"   /* TIM_HandleTypeDef / GPIO_TypeDef */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化执行器:加热片 compare 先归零,再启动 PWM(上电零输出不变量)
 * @param htim        加热片 PWM 定时器(htim8,CH1=罐侧 CH2=管侧,实现内固定)
 * @param alarm_port  超压告警 GPIO 端口(ALARM_EN)
 * @param alarm_pin   超压告警 GPIO 引脚
 * @param valve_port  电磁阀使能 GPIO 端口(PRESSURE_CTRL_EN)
 * @param valve_pin   电磁阀使能 GPIO 引脚
 * @return HAL_OK=两通道已启动;HAL_ERROR=任一通道启动失败;HAL_BUSY=其余情况
 */
HAL_StatusTypeDef CoolingActuator_Init(TIM_HandleTypeDef *htim,
                                       GPIO_TypeDef *alarm_port, uint16_t alarm_pin,
                                       GPIO_TypeDef *valve_port, uint16_t valve_pin);

/**
 * @brief 设置加热片两通道占空比(0 = 关断;满量程见 TIM8 ARR=10000)
 * @note  正常路径由控制事件按配置单源调用;超压时的立即关断走 OverpressureLatch。
 */
void CoolingActuator_HeaterSetDuty(uint32_t duty_ch1, uint32_t duty_ch2);

/**
 * @brief 电磁阀使能(独立于加热/超压,由管道压力阈值决定)
 */
void CoolingActuator_ValveSetEnable(uint8_t enable);

/**
 * @brief 超压锁存(ISR 安全):加热片两通道立即归零 + ALARM 置位
 * @note  由压力采样 ISR 快路径调用,不等主循环事件。
 */
void CoolingActuator_OverpressureLatch(void);

/**
 * @brief 超压解除:仅清 ALARM;加热片恢复属控制策略,由控制事件 HeaterSetDuty 决定
 */
void CoolingActuator_OverpressureClear(void);

#ifdef __cplusplus
}
#endif

#endif /* COOLING_ACTUATOR_H */
