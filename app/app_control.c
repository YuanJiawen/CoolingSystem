#include "app_control.h"
#include "main.h"       // 包含 HAL 库和硬件引脚定义
#include "lvgl.h"       // 包含 LVGL 库
#include "cooling_ui.h"
#include "cooling_control.h"
#include "cooling_control_config.h"
/* 外部硬件句柄声明 (根据你的实际工程可能需要修改) */
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim8;
extern TIM_HandleTypeDef htim11;
extern TIM_HandleTypeDef htim13;
extern TIM_HandleTypeDef htim14;

/* 冷却控制实例(每压力通道一个),配置见 cooling_control_config.h(唯一权威源) */
static CoolingCtrl cooling_ctrls[COOLING_CTRL_CHANNELS];

/* 设定目标压力值 (PSI)，可由上位机或LVGL界面修改 */
float target_pressure_ch1 = 50.0f; 
float target_pressure_ch2 = 50.0f;

/* 配置与定时器校验失败标志(1 = 配置不一致,需检查 tim.c;调试时可用调试器观察) */
static volatile uint8_t cooling_config_mismatch = 0;

/* ========================== 私有辅助函数 ========================== */

/* ================= ADC 中断采样(超压快路径) =================
 * 设计:500ms 事件异步启动采样,转换完成后在中断回调内
 *   换算压力 -> 立即判超压 -> 置/清 ALARM GPIO,
 *   不依赖事件队列,超压告警延迟降至微秒级(采样时间)。
 * 控制与 UI 仍在主循环,消费 pressure_snapshot 快照。
 */
static volatile uint8_t  adc_sampling    = 0;  /* 1=本轮采样进行中 */
static volatile uint8_t  adc_chan_phase  = 0;  /* 0=通道1, 1=通道2 */
static volatile float    pressure_snapshot[2]; /* 最近一次采样压力(PSI) */
static volatile uint8_t  adc_data_valid  = 0;  /* 快照可用标志 */

/**
 * @brief 配置指定通道并启动中断采样(规避 HAL 多通道轮询错位的坑)
 */
static void App_ADC_StartChannel(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    sConfig.Channel = channel;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;

    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) == HAL_OK) {
        if (HAL_ADC_Start_IT(&hadc1) != HAL_OK) {
            adc_sampling = 0; /* 启动失败,结束本轮采样,避免采样冻结 */
        }
    } else {
        adc_sampling = 0; /* 配置失败,结束本轮采样 */
    }
}

/**
 * @brief 启动一轮双通道采样(通道1 先,完成回调中串行启动通道2)
 */
static void App_ADC_StartSampling(void)
{
    adc_sampling   = 1;
    adc_chan_phase = 0;
    App_ADC_StartChannel(ADC_CHANNEL_1);
}

/**
 * @brief ADC 转换完成中断回调(超压快路径核心,ISR 上下文)
 * @note  UI 更新不在此处做(LVGL 非中断安全),由主循环经快照同步。
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance != ADC1) return;

    uint32_t raw = HAL_ADC_GetValue(hadc);

    if (adc_chan_phase == 0) {
        /* 通道1 完成:立即置位(若超压),然后启动通道2 */
        pressure_snapshot[0] = CoolingControl_PressureFromAdc(raw);
        if (pressure_snapshot[0] > COOLING_CTRL_OVERPRESSURE_PSI) {
            HAL_GPIO_WritePin(ALARM_EN_GPIO_Port, ALARM_EN_Pin, GPIO_PIN_SET);
        }
        adc_chan_phase = 1;
        App_ADC_StartChannel(ADC_CHANNEL_2);
    } else {
        /* 通道2 完成:综合两通道判定(可清除告警),本轮采样结束 */
        pressure_snapshot[1] = CoolingControl_PressureFromAdc(raw);
        if ((pressure_snapshot[0] > COOLING_CTRL_OVERPRESSURE_PSI) ||
            (pressure_snapshot[1] > COOLING_CTRL_OVERPRESSURE_PSI)) {
            HAL_GPIO_WritePin(ALARM_EN_GPIO_Port, ALARM_EN_Pin, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(ALARM_EN_GPIO_Port, ALARM_EN_Pin, GPIO_PIN_RESET);
        }
        adc_sampling   = 0;
        adc_data_valid = 1;
    }
}

/* ========================== 事件回调函数 (消费者) ========================== */

/**
 * @brief 需求1：处理 ADC 采集与 PID 更新 (500ms)
 * @note  薄适配:采样 -> 换算 -> 控制律(纯计算) -> 写硬件 -> 快照转发 UI。
 *        超压快路径(中断内判断)为后续改造,当前仍在周期事件内但已提前到控制律之前。
 */
static uint8_t on_adc_pid_event(const Event_t* evt)
{
    (void)evt;
    const CoolingCtrlConfig *cfg = &cooling_control_default_config;

    /* 1. 异步启动采样(中断完成时立即判超压置 GPIO,不阻塞主循环) */
    if (!adc_sampling) {
        App_ADC_StartSampling();
    }

    /* 2. 用最近一次采样快照做控制(首次上电前 adc_data_valid=0,按 0 压力处理) */
    float pressure_1 = adc_data_valid ? pressure_snapshot[0] : 0.0f;
    float pressure_2 = adc_data_valid ? pressure_snapshot[1] : 0.0f;

    /* 3. 控制律:每通道步进(纯计算,不触碰硬件) */
    CoolingCtrlOut out_1 = CoolingControl_Step(&cooling_ctrls[0], cfg, target_pressure_ch1, pressure_1);
    CoolingCtrlOut out_2 = CoolingControl_Step(&cooling_ctrls[1], cfg, target_pressure_ch2, pressure_2);

    /* 4. 写硬件 (adapter): PWM 输出 */
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, (uint32_t)out_1.pwm);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, (uint32_t)out_2.pwm);

    /* 5. 写硬件 (adapter): 电磁阀使能 + UI 状态同步(修复:阀状态此前从不更新) */
    uint8_t valve_enable = CoolingControl_ValveEnable(cfg, pressure_2);
    if (valve_enable) {
        HAL_GPIO_WritePin(PRESSURE_CTRL_EN_GPIO_Port, PRESSURE_CTRL_EN_Pin, GPIO_PIN_SET);
        cooling_ui_set_valve_state(VALVE_OPENED);
    } else {
        HAL_GPIO_WritePin(PRESSURE_CTRL_EN_GPIO_Port, PRESSURE_CTRL_EN_Pin, GPIO_PIN_RESET);
        cooling_ui_set_valve_state(VALVE_CLOSED);
    }

    /* 6. 状态快照转发 UI(超压 GPIO 已在中断内实时置位,这里同步告警 UI) */
    float display_pressure_1 = (pressure_1 < 1.0f) ? 0.0f : pressure_1;
    float display_pressure_2 = (pressure_2 < 1.0f) ? 0.0f : pressure_2;
    cooling_ui_set_tank_pressure(display_pressure_1);
    cooling_ui_set_pipe_pressure(display_pressure_2);

    if (out_1.tank_connected) {
        cooling_ui_set_tank_connection(TANK_CONNECTED);
    } else {
        cooling_ui_set_tank_connection(TANK_DISCONNECTED);
    }

    if (out_1.overpressure || out_2.overpressure) {
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
    
    /* 2. 配置单源校验 (候选 2):配置头 vs 定时器实际值
     *    - TIM8 ARR+1 必须等于 PWM 上限(10000)
     *    - TIM11 实际中断周期必须等于控制周期(500ms)
     *    修复:此前第三参误传配置宏,校验恒真;现改为按 htim11 实际参数计算。
     *    不一致时置标志,可在调试器中观察 cooling_config_mismatch。
     */
    uint32_t tim11_clk_hz = HAL_RCC_GetPCLK2Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE2) != RCC_CFGR_PPRE2_DIV1) {
        tim11_clk_hz *= 2U; /* APB2 预分频 >1 时定时器时钟 = PCLK2 * 2 */
    }
    uint32_t tim11_period_ms = (uint32_t)(((uint64_t)(htim11.Init.Prescaler + 1U) *
                                           (uint64_t)(htim11.Init.Period + 1U) * 1000ULL) /
                                          tim11_clk_hz);
    if (CoolingControl_ValidateConfig(&cooling_control_default_config,
                                      htim8.Init.Period,
                                      tim11_period_ms) == 0) {
        cooling_config_mismatch = 1;
    }

    /* 3. 初始化冷却控制实例(两通道共用默认配置) */
    for (uint8_t i = 0; i < COOLING_CTRL_CHANNELS; i++) {
        CoolingControl_Init(&cooling_ctrls[i], &cooling_control_default_config);
    }

    /* 4. 注册事件回调 */
    evt_register_handler(APP_EVT_ADC_PID, on_adc_pid_event);
    evt_register_handler(APP_EVT_LVGL_TICK, on_lvgl_tick_event);
    evt_register_handler(APP_EVT_VALVE_CHECK, on_valve_check_event);
    
    /* 5. 启动 PWM 与采样定时器 */
    HAL_StatusTypeDef rtv_ch1, rtv_ch2;
    HAL_TIM_Base_Start_IT(&htim11);
    HAL_TIM_Base_Start_IT(&htim13);
    HAL_TIM_Base_Start_IT(&htim14);
    rtv_ch1 = HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);
    rtv_ch2 = HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2);

    /* 上电安全:立即清零 PWM 输出(tim.c 中 Pulse=2000 会在启动瞬间输出 20% 占空比,
     * 首个控制周期前不允许非零输出)。 */
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, 0U);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, 0U);
		
    if (rtv_ch1 == HAL_OK && rtv_ch2 == HAL_OK) {
        cooling_ui_set_heater_state(HEATER_ON);
    } else if (rtv_ch1 == HAL_ERROR || rtv_ch2 == HAL_ERROR) {
        cooling_ui_set_heater_state(HEATER_ERROR);
    } else {
        cooling_ui_set_heater_state(HEATER_OFF);
    }
}

/* 以下函数请在 stm32xxx_it.c 中的对应定时器中断服务函数中调用 */

void App_ISR_TIM11_500ms(void) {
    // 发布 ADC/PID 事件 (参数和指针都传 0/NULL 即可)
    evt_publish_unique(APP_EVT_ADC_PID, 0, NULL);
}

void App_ISR_TIM13_5ms(void) {
    // 注意:TIM13 实际中断周期为 10ms(CubeMX 默认 Prescaler=180-1, Period=5000-1,
    //       APB1 定时器时钟 90MHz -> 10ms;函数名沿旧,勿据此推断周期)。
    //       按实际 10ms 喂给 LVGL 时基(修复:此前喂 5ms 导致 LVGL 时间流速减半)。
    //       若要真 5ms,请在 CubeMX 中改 TIM13 Period=2500-1。
    lv_tick_inc(10);
    // 发布调度事件让主循环执行 lv_task_handler()
    evt_publish_unique(APP_EVT_LVGL_TICK, 0, NULL);
}

void App_ISR_TIM14_1s(void) {
    evt_publish_unique(APP_EVT_VALVE_CHECK, 0, NULL);
	
}
