/**
 * @file    sim_test.h
 * @brief   冷却系统模拟测试(虚拟数据持续驱动 UI)
 *
 * 宏 SIM_TEST_ENABLE 控制测试模式:
 *   1 = 测试模式(压力三角波扫描 + 随机设备状态,不进工作流程)
 *   0 = 正常工作模式
 */

#ifndef SIM_TEST_H
#define SIM_TEST_H

/* ==================== 测试开关(1 开启, 0 关闭) ==================== */
#define SIM_TEST_ENABLE  0

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 模拟测试初始化(创建 LVGL 定时器,持续驱动 UI)
 * @note  仅在 #if SIM_TEST_ENABLE 时调用,不启动事件框架与 ADC/PID。
 *        主循环只需循环调用 lv_task_handler()。
 */
void App_Simulation_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* SIM_TEST_H */
