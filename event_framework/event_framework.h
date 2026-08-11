/**
 * @file    event_framework.h
 * @brief   基于 lwrb 环形缓冲区的事件驱动框架(ISR-safe 消息队列 + 按 event_id 分发)
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

/**
 * @brief 事件回调函数（按 event_id 注册）
 * @return 1: 事件已被消费; 0: 未消费（当前框架不区分二者，返回值保留供调用方表达）
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

/**
 * @brief 发布唯一事件：同一个 event_id 尚未被消费前不会重复入队。
 * @note  适合周期 tick/采样/轮询事件，避免主循环偶发变慢时队列被同类事件填满。
 * @note  注意:同一 event_id 请勿混用 evt_publish 与 evt_publish_unique,
 *        普通发布不会置 pending 位,混用会破坏去重语义。
 */
int evt_publish_unique(uint8_t event_id, uint16_t param, void* data_ptr);
int evt_publish_unique_event(const Event_t* evt);

int evt_poll(Event_t* evt);

int evt_register_handler(uint8_t event_id, EventHandler_fn handler);

/**
 * @brief 注册事件溢出丢弃钩子
 */
void evt_register_drop_hook(EventDropHandler_fn drop_hook);

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
