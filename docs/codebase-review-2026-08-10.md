# 代码库审查记录(2026-08-10)

本文档记录一次全库审查发现的潜在 bug 与冗余代码,供后续处理。**冗余代码暂不删除**(用户要求记录即可)。

## 一、潜在 Bug 清单

### 已修复(本轮)

| #   | 严重度 | 问题                                                                                                     | 修复                                                              |
| --- | --- | ------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------- |
| A1  | 严重  | TIM13 实际中断周期 10ms(CubeMX PSC=180-1/ARR=5000-1,APB1 Tim 90MHz),但 `lv_tick_inc(5)` 按 5ms 喂 → LVGL 时基流速减半 | `app/app_control.c` `lv_tick_inc(5)→(10)`,注释说明;tim.c 注释同步       |
| A2  | 严重  | TIM14 实际周期 2s(PSC=18000-1/ARR=10000-1),预期 1s → 液位检查延迟翻倍                                                | `Core/Src/tim.c` TIM14 `Period 10000-1→5000-1`(CubeMX 需同步)      |
| A3  | 严重  | TIM8 PWM 上电 Pulse=2000(20% 占空比),首个控制周期前非零输出(安全风险)                                                      | `App_Control_Init` PWM 启动后立即 `SET_COMPARE(0)` 清零两通道             |
| A4  | 中等  | 配置校验第三参误传配置宏(500),校验恒真;TIM13/TIM14 周期错误未被发现                                                            | 改为按 `htim11.Init.Prescaler/Period` + APB2 时钟计算实际周期传入            |
| A5  | 中等  | 电磁阀 UI 状态从不更新(`cooling_ui_set_valve_state` 被 linker 移除)                                                | `on_adc_pid_event` 写入 GPIO 时同步 `set_valve_state(OPENED/CLOSED)` |
| A6  | 中等  | `HAL_ADC_Start_IT` 返回值未检查,失败则采样冻结、ALARM GPIO 卡旧状态                                                      | `App_ADC_StartChannel` 检查返回值,失败置 `adc_sampling=0`               |

### 未修复(记录,待决定)

| #   | 严重度 | 问题                                                  | 建议                            |
| --- | --- | --------------------------------------------------- | ----------------------------- |
| A7  | 中等  | PID 反馈滞后一个采样周期(用上一轮快照计算)                            | 采样完成事件驱动 PID,或文档化接受           |
| A8  | 中等  | 液位异常分支(上液下空)静默无反馈                                   | else 分支加日志/UI 故障状态(需新增 UI 状态) |
| A9  | 中等  | LVGL 时基连带(随 A1 修复已解决)                               | —                             |
| A10 | 轻微  | 压力 <1 PSI 显示归零,控制用真实值(不一致)                          | 文档化或统一                        |
| A11 | 轻微  | `target_pressure_ch1/2` 全局无外部修改入口,float 无并发保护       | 提供 setter 或改 static           |
| A12 | 轻微  | PID 微分系数在 `2*tau<T` 为负(Kd>0 时递推不稳)                  | 要求 `tau>T/2` 或换微分实现           |
| A13 | 轻微  | 压力换算注释(Vref=3V)与实现(0~6V 满量程)不符                      | 修正注释(硬件标定确认)                  |
| A14 | 轻微  | SDRAM 刷新率按 90MHz 注释计算,FMC 时钟可能 180MHz               | 核实 FMC 时钟                     |
| A15 | 轻微  | printf 调试被禁用(DEBUG_USART_PRINTF_ENABLE=0),SD 错误日志静默 | 测试期打开,发布关闭                    |
| A16 | 轻微  | 触摸已删但 EXTI15_10(PD13 触摸中断)仍使能无消费方                   | CubeMX 层面移除                   |
| A17 | 轻微  | ISR 内 float 运算 + 快照新旧混读窗口(不影响安全)                    | 文档化                           |
| A18 | 轻微  | `HAL_TIM_Base_Start_IT` 返回值未检查                      | 检查并置错误标志                      |

## 二、冗余代码清单(记录,未删除)

| #                | 位置                                          | 说明                                                                                    | 状态               |
| ---------------- | ------------------------------------------- | ------------------------------------------------------------------------------------- | ---------------- |
| <mark>B1</mark>  | `bsp_sdio/sd_test.c/h`                      | `SD_StressTest` 无调用方                                                                  | 用户选择保留(诊断工具)     |
| <mark>B2</mark>  | `FatFs/src/fatfs_test.c`                    | `FatFs_IntegrationTest` 未调用;main.c 有 2 行重复注释                                          | 可删               |
| <mark>B3</mark>  | `ff15/` 整个目录                                | 与 `FatFs/src/` 重复的 FatFS 源码拷贝,未参与构建                                                   | 可删               |
| B4               | `delay/delay_us.c/h`                        | `HAL_Delay_us` 无调用方                                                                   | 用户选择保留(工具)       |
| B5               | `usart/bsp_debug_usart.c`                   | `Usart_SendString`/`fgetc` 无调用;`extern ucTemp` 悬空                                     | 可删               |
| B6               | `usart/bsp_debug_usart.h`                   | RCC_UARTxCLKSOURCE 宏无引用                                                               | 可删               |
| <mark>B7</mark>  | `demo_lvgl/cooling_ui.c`                    | `cooling_ui_show_logo_only()` 无调用(linker 移除)                                          | 可删               |
| B8               | `event_framework/event_framework.c`         | `evt_publish`/`evt_register_drop_hook`/`evt_get_pending_count`/`evt_queue_flush` 无消费者 | 可删               |
| <mark>B9</mark>  | `lwrb/lwrb/lwrb_ex.c`                       | 整个文件未使用                                                                               | 可删               |
| B10              | `fonts lib/my_font_chinese_32.c`            | 无引用(linker 移除)                                                                        | 可删               |
| <mark>B11</mark> | `img_lib/`                                  | 未参与编译(图标实际从 SD 加载)                                                                    | 可删               |
| B12              | `MDK-ARM/RTE/LVGL/lv_port_indev_template.*` | 触摸输入已移除,文件被 Keil RTE 恢复为 #if 0 模板                                                     | 需 Keil RTE 管理器移除 |
| <mark>B13</mark> | `MDK-ARM/Demo_F429_DDS/`                    | 历史 .o/.d 残留(bsp_multibutton/driver_w25qxx 等)                                          | 可清理              |
| <mark>B14</mark> | `Core/Src/main.c`                           | `#include "lv_demo_widgets.h"` 未使用;重复注释行                                              | 可清理              |
| B15              | `app/app_control.c`                         | `cooling_config_mismatch` 只写不读(调试用)                                                   | 保留或加查询接口         |
| B16              | `app/app_control.c`                         | `target_pressure_ch1/2` 无外部写入入口                                                       | 同 A11            |

## 三、Keil RTE 触摸残留说明

`lv_port_indev_template.c/.h` 位于 Keil 管理的 RTE 目录,删除后 Keil 重开工程会自动从 pack 恢复(当前为 `#if 0` 未启用模板,不参与编译、无功能影响)。**彻底移除需在 Keil 的 Manage Run-Time Environment 中取消 LVGL 的 Porting 组件**。用户代码(`bsp_touch.*`)已完全删除且工程条目已移除。
