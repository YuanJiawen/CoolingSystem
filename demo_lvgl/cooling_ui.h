/**
 * @file cooling_ui.h
 * @brief 冷却系统实时监控面板 UI 接口声明
 *        MCU: STM32F429IG | LVGL 8.x | 800x480 RGB
 */

#ifndef COOLING_UI_H
#define COOLING_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/* ======================== 枚举定义 ======================== */

/** 冷却剂余量状态 */
typedef enum {
    COOLANT_LEVEL_FULL = 0,   /**< 充足 */
    COOLANT_LEVEL_OK,          /**< 尚可 */
    COOLANT_LEVEL_EMPTY        /**< 耗尽 */
} coolant_level_t;

/** 冷却罐连接状态 */
typedef enum {
    TANK_DISCONNECTED = 0,     /**< 未连接 */
    TANK_CONNECTED             /**< 已连接 */
} tank_conn_state_t;

/** 电磁阀状态 */
typedef enum {
    VALVE_FAULT = 0,           /**< 故障 */
    VALVE_CLOSED,              /**< 关闭 */
    VALVE_OPENED               /**< 已打开 */
} valve_state_t;

/** 加热片状态 */
typedef enum {
    HEATER_OFF = 0,            /**< 未开启加热 */
    HEATER_ON,                 /**< 已开启加热 */
		HEATER_ERROR							 /**< 故障 */
} heater_state_t;

/* ======================== 接口函数 ======================== */

/**
 * @brief  创建冷却系统监控面板 UI（调用一次即可）
 * @note   内部会创建所有控件并布局到屏幕上
 */
void cooling_ui_show_logo_only(void);
void cooling_ui_create(void);

/**
 * @brief  更新冷却罐压力仪表盘
 * @param  pressure  压力值，单位 PSI，范围 0~150，保留1位小数
 */
void cooling_ui_set_tank_pressure(float pressure);

/**
 * @brief  更新管路压力仪表盘
 * @param  pressure  压力值，单位 PSI，范围 0~150，保留1位小数
 */
void cooling_ui_set_pipe_pressure(float pressure);

/**
 * @brief  显示全屏压力过大警告（闪烁文本 + 警告图标）
 * @note   当任一压力 > 130 PSI 时调用
 */
void cooling_ui_show_overpressure_warning(void);

/**
 * @brief  隐藏全屏压力过大警告
 */
void cooling_ui_hide_overpressure_warning(void);

/**
 * @brief  更新冷却剂余量状态
 * @param  level  coolant_level_t 枚举值
 */
void cooling_ui_set_coolant_level(coolant_level_t level);

/**
 * @brief  更新冷却罐连接状态（含瓶子图标颜色 + 文本）
 * @param  state  tank_conn_state_t 枚举值
 */
void cooling_ui_set_tank_connection(tank_conn_state_t state);

/**
 * @brief  更新电磁阀状态（LED指示灯 + 文本）
 * @param  state  valve_state_t 枚举值
 */
void cooling_ui_set_valve_state(valve_state_t state);

/**
 * @brief  更新加热片状态（LED指示灯 + 文本）
 * @param  state  heater_state_t 枚举值
 */
void cooling_ui_set_heater_state(heater_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* COOLING_UI_H */