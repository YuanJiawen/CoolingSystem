#ifndef APP_CONTROL_H
#define APP_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "event_framework.h"

/* 利用框架预留的自定义事件起始地址定义应用事件 */
typedef enum {
    APP_EVT_CONTROL = EVT_USER_CUSTOM_START,    // 500ms事件(采样+加热+阀)
    APP_EVT_LVGL_TICK,                          // 10ms事件(LVGL 时基+调度)
    APP_EVT_VALVE_CHECK                         // 1s事件(液位检查)
} AppEventId_e;

/* ========================== API 接口 ========================== */

/**
 * @brief  应用层初始化（事件框架 + 冷却执行器 + 压力采样适配器 + 事件注册等）
 */
void App_Control_Init(void);

/**
 * @brief  硬件定时器中断钩子函数（供 stm32xxx_it.c 或 HAL 回调调用）
 * @note   函数名周期与实际定时器配置一致(TIM13: PSC=180-1/ARR=5000-1 = 10ms)
 */
void App_ISR_TIM11_500ms(void);
void App_ISR_TIM13_10ms(void);
void App_ISR_TIM14_1s(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CONTROL_H */