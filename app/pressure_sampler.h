/**
 * @file    pressure_sampler.h
 * @brief   压力采样适配器(深模块:双通道 ADC 串行采样 + 超压快路径)
 *
 * 设计要点(架构评审 2026-08-17,候选 1):
 * - 深模块:小 interface(Start/IsBusy/GetSnapshot),集中实现
 *   双通道序列化、ADC 换算、超压判定与 ALARM GPIO。
 * - ISR(HAL_ADC_ConvCpltCallback)由本模块接管,宿主不感知采样时序。
 * - 双缓冲:ISR 写进行中缓冲,一轮完整完成后一次性发布快照,
 *   GetSnapshot 只读已发布快照,杜绝「新 ch1 + 旧 ch2」混读竞态。
 * - 纯硬件 adapter:依赖 HAL(ADC/GPIO)与 cooling_control 的换算函数。
 */
#ifndef PRESSURE_SAMPLER_H
#define PRESSURE_SAMPLER_H

#include <stdint.h>
#include <stddef.h>          /* NULL */
#include "stm32f4xx_hal.h"   /* ADC_HandleTypeDef / GPIO_TypeDef */
#include "cooling_control.h" /* CoolingCtrlConfig */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 一轮完整采样的结果快照(只读,宿主消费)
 */
typedef struct {
    float   pressure[2];   /**< [0]=罐压(ch1),[1]=管压(ch2),单位 PSI */
    uint8_t valid;         /**< 1: 已有一轮完整快照 */
    uint8_t overpressure;  /**< 1: 与快照同轮的任一通道超压 */
} PressureSnapshot;

/**
 * @brief 初始化采样器(单例,对应唯一 ADC1)
 * @param hadc       ADC 句柄(hadc1)
 * @param cfg        冷却控制配置(取超压阈值)
 * @param alarm_port 超压告警 GPIO 端口(ALARM_EN)
 * @param alarm_pin  超压告警 GPIO 引脚(ALARM_EN)
 */
void    PressureSampler_Init(ADC_HandleTypeDef *hadc, const CoolingCtrlConfig *cfg,
                             GPIO_TypeDef *alarm_port, uint16_t alarm_pin);

/**
 * @brief 启动新一轮双通道采样(幂等:仅空闲时生效)
 * @note  由宿主在控制周期事件(如 500ms TIM11)中调用;
 *        采样在中断内异步完成,超压时立即置/清 ALARM GPIO。
 */
void    PressureSampler_Start(void);

/**
 * @brief 采样是否进行中
 */
uint8_t PressureSampler_IsBusy(void);

/**
 * @brief 取最近一次完整快照(首次上电前 valid=0)
 */
void    PressureSampler_GetSnapshot(PressureSnapshot *out);

#ifdef __cplusplus
}
#endif

#endif /* PRESSURE_SAMPLER_H */
