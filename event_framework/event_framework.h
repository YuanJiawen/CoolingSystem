/**
 * @file    event_framework.h
 * @brief   基于 lwrb 环形缓冲区的事件驱动框架（含有限状态机）
 */

#ifndef EVENT_FRAMEWORK_H
#define EVENT_FRAMEWORK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "cmsis_compiler.h"

/* ========================== 用户可配置区域 ========================== */
#define EVT_QUEUE_CAPACITY      32      // 事件缓冲区最大容量

/* ========================== 事件ID定义 ========================== */
typedef enum {
    EVT_NONE = 0,
    /* --- 硬件层事件 --- */
    EVT_KEY_PRESS,
    EVT_KEY_RELEASE,
    EVT_UART_RX,
    EVT_UART_TX_COMPLETE,
    EVT_ADC_COMPLETE,
    EVT_TIMER_TICK,
    EVT_EXTI_TRIGGER,
    /* --- 应用层事件 --- */
    EVT_SENSOR_DATA_READY,
    EVT_COMM_CONNECTED,
    EVT_COMM_DISCONNECTED,
    EVT_ERROR,
    EVT_STATE_TIMEOUT,
    
    EVT_USER_CUSTOM_START = 0x80,
    EVT_MAX = 0xFF
} EventId_e;

/* ========================== 事件结构体 ========================== */
typedef struct {
    uint8_t   event_id;     /**< 事件类型 */
    uint16_t  param;        /**< 简单参数 */
    void* data_ptr;     /**< 复杂数据指针，动态内存分配时需注意生命周期 */
} Event_t;

/* ========================== 系统状态机定义 ========================== */
typedef enum {
    SYS_STATE_INIT = 0,
    SYS_STATE_IDLE,
    SYS_STATE_RUNNING,
    SYS_STATE_ERROR,
    SYS_STATE_SLEEP,
    SYS_STATE_MAX
} SysState_e;

/**
 * @brief 状态机处理函数
 * @return 处理后应转移到的新状态
 */
typedef SysState_e (*StateHandler_fn)(const Event_t* evt);

/**
 * @brief 全局事件回调函数
 * @return 1: 该事件已被完全消费（拦截），不要再传给状态机; 0: 放行，继续传给状态机
 */
typedef uint8_t (*EventHandler_fn)(const Event_t* evt);

/**
 * @brief 事件被覆盖（丢弃）时的回调函数
 * @note 用于释放 data_ptr 对应的动态内存，防止内存泄漏
 */
typedef void (*EventDropHandler_fn)(const Event_t* dropped_evt);

/* ========================== 框架API接口 ========================== */
int evt_framework_init(void);

/**
 * @brief 发布事件（ISR-Safe）
 */
int evt_publish(uint8_t event_id, uint16_t param, void* data_ptr);
int evt_publish_event(const Event_t* evt);

int evt_poll(Event_t* evt);

int evt_register_handler(uint8_t event_id, EventHandler_fn handler);
int evt_register_state_handler(SysState_e state, StateHandler_fn handler);

/**
 * @brief 注册事件溢出丢弃钩子
 */
void evt_register_drop_hook(EventDropHandler_fn drop_hook);

void evt_set_state(SysState_e state);
SysState_e evt_get_state(void);

void evt_dispatch(void);

uint32_t evt_get_pending_count(void);
void evt_queue_flush(void);

/* ========================== 平台相关临界区宏 ========================== */
#define EVT_ENTER_CRITICAL()                       \
    uint32_t evt_primask = __get_PRIMASK();        \
    __disable_irq()

#define EVT_EXIT_CRITICAL()                        \
    do {                                           \
        if (evt_primask == 0U) {                   \
            __enable_irq();                        \
        }                                          \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* EVENT_FRAMEWORK_H */
