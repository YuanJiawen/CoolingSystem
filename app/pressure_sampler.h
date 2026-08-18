/**
 * @file    pressure_sampler.h
 * @brief   压力采样适配器(深模块:双通道 ADC 串行采样 + 超压快路径)
 *
 * 设计要点(架构评审 2026-08-17/2026-08-18):
 * - 深模块:小 interface(Start/IsBusy/GetSnapshot),集中实现
 *   双通道序列化、ADC 换算、超压判定;超压动作(加热片归零 + ALARM)
 *   委托冷却执行器,采样器不再持有告警引脚。
 * - ISR(HAL_ADC_ConvCpltCallback)由本模块接管,宿主不感知采样时序。
 * - 双缓冲 + 临界区:ISR 写进行中缓冲,一轮完整后在 PRIMASK 临界区内
 *   发布快照,GetSnapshot 于临界区内拷贝 —— 发布/读取原子,杜绝撕裂。
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
 * @param hadc ADC 句柄(hadc1)
 * @param cfg  冷却控制配置(取超压阈值)
 */
void    PressureSampler_Init(ADC_HandleTypeDef *hadc, const CoolingCtrlConfig *cfg);

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
