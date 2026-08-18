#include "app_control.h"
#include "main.h"       // 包含 HAL 库和硬件引脚定义
#include "lvgl.h"       // 包含 LVGL 库
#include "cooling_ui.h"
#include "cooling_control.h"
#include "cooling_control_config.h"
#include "cooling_actuator.h"
#include "pressure_sampler.h"
/* 外部硬件句柄声明 (根据你的实际工程可能需要修改) */
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim8;
extern TIM_HandleTypeDef htim11;
extern TIM_HandleTypeDef htim13;
extern TIM_HandleTypeDef htim14;

/* ========================== 事件回调函数 (消费者) ========================== */

/**
 * @brief 需求1：处理 ADC 采样、加热片输出与电磁阀 (500ms)
 * @note  加热片为开环固定占空比(无 PID);超压时关断加热片(两通道 PWM 归零),
 *        但不动电磁阀(阀由管道压力阈值独立控制)。
 */
static uint8_t on_control_event(const Event_t* evt)
{
    (void)evt;
    const CoolingCtrlConfig *cfg = &cooling_control_default_config;

    /* 1. 异步启动新一轮采样(幂等;中断完成时立即判超压置 GPIO,不阻塞主循环) */
    PressureSampler_Start();

    /* 2. 取最近一次完整快照(首次上电前 valid=0,按 0 压力处理) */
    PressureSnapshot snap;
    PressureSampler_GetSnapshot(&snap);
    float pressure_1 = snap.valid ? snap.pressure[0] : 0.0f;
    float pressure_2 = snap.valid ? snap.pressure[1] : 0.0f;

    /* 3. 安全门控:超压 → 关断加热片(两通道 PWM 归零);否则写固定占空比
     *    (执行器单写者;ISR 内已先行锁存,此处按快照维持/恢复) */
    uint8_t overpressure = CoolingControl_IsOverpressure(cfg, pressure_1, pressure_2);
    CoolingActuator_HeaterSetDuty(overpressure ? 0U : (uint32_t)COOLING_CTRL_HEATER_DUTY_CH1,
                                  overpressure ? 0U : (uint32_t)COOLING_CTRL_HEATER_DUTY_CH2);

    /* 4. 电磁阀使能(独立于加热/超压,基于管道压力阈值)+ UI 状态同步 */
    uint8_t valve_enable = CoolingControl_ValveEnable(cfg, pressure_2);
    CoolingActuator_ValveSetEnable(valve_enable);
    cooling_ui_set_valve_state(valve_enable ? VALVE_OPENED : VALVE_CLOSED);

    /* 5. 状态快照转发 UI(超压 GPIO 已在中断内实时置位,这里同步告警 UI);
      *    控制判定(上面 3/4 步)用原始压力,显示走单源钳位 */
    cooling_ui_set_tank_pressure(CoolingControl_DisplayPressure(pressure_1));
    cooling_ui_set_pipe_pressure(CoolingControl_DisplayPressure(pressure_2));

    if (CoolingControl_IsTankConnected(cfg, pressure_1)) {
        cooling_ui_set_tank_connection(TANK_CONNECTED);
    } else {
        cooling_ui_set_tank_connection(TANK_DISCONNECTED);
    }

    if (overpressure) {
        cooling_ui_show_overpressure_warning();
    } else {
        cooling_ui_hide_overpressure_warning();
    }

    HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin); // debug 用，不用可删除

    return 1; /* 事件拦截 */
}

/**
 * @brief 需求2：处理 LVGL 调度 (5ms)
 */
static uint8_t on_lvgl_tick_event(const Event_t* evt) {
    // LVGL 核心重绘及任务调度
    lv_task_handler();
    return 1;
}

/**
 * @brief 需求3：检查液位与电磁阀 (1s)
 */
static uint8_t on_valve_check_event(const Event_t* evt)
{
    (void)evt;

    GPIO_PinState level_state_high = HAL_GPIO_ReadPin(LEVEL_HIGH_FB_GPIO_Port, LEVEL_HIGH_FB_Pin);
    GPIO_PinState level_state_low  = HAL_GPIO_ReadPin(LEVEL_LOW_FB_GPIO_Port, LEVEL_LOW_FB_Pin);

    /* 传感器规则：
     * 有液体 -> 低电平
     * 无液体 -> 高电平
     */

    /* 1. 上下两个都检测到液体 -> 充足 */
    if ((level_state_high == GPIO_PIN_RESET) &&
        (level_state_low  == GPIO_PIN_RESET)) {

        cooling_ui_set_coolant_level(COOLANT_LEVEL_FULL);
    }
    /* 2. 上面没有液体，下面有液体 -> 尚可 */
    else if ((level_state_high == GPIO_PIN_SET) &&
             (level_state_low  == GPIO_PIN_RESET)) {

        cooling_ui_set_coolant_level(COOLANT_LEVEL_OK);
    }
    /* 3. 上下两个都没有液体 -> 耗尽 */
    else if ((level_state_high == GPIO_PIN_SET) &&
             (level_state_low  == GPIO_PIN_SET)) {

        cooling_ui_set_coolant_level(COOLANT_LEVEL_EMPTY);
    }
    /* 可选保护分支：
     * 上面有液体、下面没液体 这种状态在物理上通常不合理，
     * 可按故障/异常处理。这里先按“充足”或“尚可”都不太严谨，
     * 建议单独记录异常。
     */
    else {
        /* 异常状态，可根据项目需要处理 */
        /* 例如：
         * cooling_ui_set_coolant_level(COOLANT_LEVEL_OK);
         * App_Log_Warn("Liquid level sensor state invalid");
         */
    }

    return 1;
}

/* ========================== 初始化与 ISR 钩子 ========================== */

void App_Control_Init(void) {
    /* 1. 初始化框架 */
    evt_framework_init();

    /* 2. 初始化冷却执行器(上电安全不变量:先归零 compare 再启动 PWM) */
    HAL_StatusTypeDef heater_rtv = CoolingActuator_Init(&htim8,
                                        ALARM_EN_GPIO_Port, ALARM_EN_Pin,
                                        PRESSURE_CTRL_EN_GPIO_Port, PRESSURE_CTRL_EN_Pin);

    /* 3. 初始化压力采样适配器(双通道 ADC;超压快路径经执行器锁存) */
    PressureSampler_Init(&hadc1, &cooling_control_default_config);

    /* 4. 注册事件回调 */
    evt_register_handler(APP_EVT_CONTROL, on_control_event);
    evt_register_handler(APP_EVT_LVGL_TICK, on_lvgl_tick_event);
    evt_register_handler(APP_EVT_VALVE_CHECK, on_valve_check_event);

    /* 5. 启动调度定时器(500ms 控制 / 10ms LVGL 时基 / 1s 液位检查) */
    HAL_TIM_Base_Start_IT(&htim11);
    HAL_TIM_Base_Start_IT(&htim13);
    HAL_TIM_Base_Start_IT(&htim14);

    if (heater_rtv == HAL_OK) {
        cooling_ui_set_heater_state(HEATER_ON);
    } else if (heater_rtv == HAL_ERROR) {
        cooling_ui_set_heater_state(HEATER_ERROR);
    } else {
        cooling_ui_set_heater_state(HEATER_OFF);
    }
}

/* 以下函数请在 stm32xxx_it.c 中的对应定时器中断服务函数中调用 */

void App_ISR_TIM11_500ms(void) {
    // 发布采样/加热/电磁阀 控制事件 (参数和指针都传 0/NULL 即可)
    evt_publish_unique(APP_EVT_CONTROL, 0, NULL);
}

void App_ISR_TIM13_10ms(void) {
    /* TIM13 实际中断周期 10ms(PSC=180-1/ARR=5000-1, APB1 定时器时钟 90MHz),
     * 按 10ms 喂 LVGL 时基(.ioc 已同步,函数名与实际周期一致)。 */
    lv_tick_inc(10);
    // 发布调度事件让主循环执行 lv_task_handler()
    evt_publish_unique(APP_EVT_LVGL_TICK, 0, NULL);
}

void App_ISR_TIM14_1s(void) {
    evt_publish_unique(APP_EVT_VALVE_CHECK, 0, NULL);

}
