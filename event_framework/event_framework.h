/**
 * @file    event_framework.h
 * @brief   基于 lwrb 环形缓冲区的事件驱动框架(ISR-safe 消息队列 + 按 event_id 分发)
 *
 * 接口收缩(架构评审 2026-08-18,候选 3):仅保留有消费者的 4 个 API
 * (init / publish_unique / register_handler / dispatch)。普通 publish、
 * drop hook、pending 计数、flush 均已按删除测试移除 —— 本代码库事件
 * 不携带动态内存(data_ptr 恒 NULL),覆盖丢弃只需清 pending 位。
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
    void* data_ptr;     /**< 复杂数据指针,动态内存分配时需注意生命周期 */
} Event_t;

/* ========================== 回调定义 ========================== */

/**
 * @brief 事件回调函数(按 event_id 注册)
 * @return 1: 事件已被消费; 0: 未消费(当前框架不区分二者,返回值保留供调用方表达)
 */
typedef uint8_t (*EventHandler_fn)(const Event_t* evt);

/* ========================== 框架API接口 ========================== */

/**
 * @brief 初始化框架(主循环启动前调用一次)
 * @return 0 成功; -1 失败
 */
int evt_framework_init(void);

/**
 * @brief 发布唯一事件(ISR-Safe):同一 event_id 尚未被消费前不会重复入队。
 * @note  适合周期 tick/采样/轮询事件,避免主循环偶发变慢时队列被同类事件填满;
 *        队列满时覆盖最旧事件(本库事件不携带动态内存,直接丢弃)。
 * @return 0 成功(含去重合并); -1 失败
 */
int evt_publish_unique(uint8_t event_id, uint16_t param, void* data_ptr);

/**
 * @brief 注册事件处理函数(按 event_id)
 * @return 0 成功; -1 参数非法
 */
int evt_register_handler(uint8_t event_id, EventHandler_fn handler);

/**
 * @brief 分发一个事件(主循环轮询调用;无事件时立即返回)
 */
void evt_dispatch(void);

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
