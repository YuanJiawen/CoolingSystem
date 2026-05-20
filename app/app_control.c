#include "app_control.h"
#include "main.h"       // 包含 HAL 库和硬件引脚定义
#include "lvgl.h"       // 包含 LVGL 库
#include "cooling_ui.h"
/* 外部硬件句柄声明 (根据你的实际工程可能需要修改) */
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim8;
extern TIM_HandleTypeDef htim11;
extern TIM_HandleTypeDef htim13;
extern TIM_HandleTypeDef htim14;

/* 全局 PID 控制器实例 */
PIDController pid_ch1;
PIDController pid_ch2;

/* 设定目标压力值 (PSI)，可由上位机或LVGL界面修改 */
float target_pressure_ch1 = 50.0f; 
float target_pressure_ch2 = 50.0f;

/* ========================== 私有辅助函数 ========================== */

/**
 * @brief 动态配置并读取指定 ADC 通道 (规避 HAL 库多通道轮询错位的坑)
 */
static uint32_t App_Read_ADC_Channel(uint32_t channel) {
    ADC_ChannelConfTypeDef sConfig = {0};
    
    sConfig.Channel = channel;
    sConfig.Rank = 1; 
    // 注意：这里的采样时间
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES; 
    
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        return 0; // 配置失败
    }

    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        uint32_t val = HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);
        return val;
    }
    
    HAL_ADC_Stop(&hadc1);
		
    return 0;
}

/**
 * @brief 将 ADC 原始值转换为压力值 (PSI)
 * @note  Vref = 3V, 分辨率 = 12位(4095)
 * Vout = 0.5V + P * 0.0266V
 */
static float App_Calculate_Pressure(uint32_t adc_val) {
    // 1. 计算电压
    float voltage = ((float)(adc_val * 2) / 4095.0f) * 3.0f;
    
    // 2. 计算压力 (V - 0.5) / 0.0266
    float pressure = (voltage - 0.5f) / 0.0266f;
    
    // 3. 负压截断保护 (滤除零点底噪)
    if (pressure < 0.0f) {
        pressure = 0.0f;
    }
    return pressure;
}

/* ========================== 事件回调函数 (消费者) ========================== */

/**
 * @brief 需求1：处理 ADC 采集与 PID 更新 (500ms)
 * @note  返回 1 表示事件已在此消费，不传入状态机
 */
static uint8_t on_adc_pid_event(const Event_t* evt)
{
    (void)evt;

    /* 1. 采集通道1和通道2 */
    uint32_t adc_val_1 = App_Read_ADC_Channel(ADC_CHANNEL_1);
    uint32_t adc_val_2 = App_Read_ADC_Channel(ADC_CHANNEL_2);

    /* 2. 转换为实际物理压力 */
    float pressure_1 = App_Calculate_Pressure(adc_val_1);
    float pressure_2 = App_Calculate_Pressure(adc_val_2);

    /* 3. 用于 UI 显示的压力值
     * 当实际压力 < 1 PSI 时，仪表盘显示 0 PSI
     */
    float display_pressure_1 = (pressure_1 < 1.0f) ? 0.0f : pressure_1;
    float display_pressure_2 = (pressure_2 < 1.0f) ? 0.0f : pressure_2;

    /* 4. 采集到压力后，立即更新 UI 仪表盘
     * pressure_1 -> 冷却罐压力
     * pressure_2 -> 管道压力
     */
    cooling_ui_set_tank_pressure(display_pressure_1);
    cooling_ui_set_pipe_pressure(display_pressure_2);

    /* 5. 根据 pressure_1 更新右侧“压力罐”连接状态
     * pressure_1 < 20 PSI  -> 未连接
     * 其他情况             -> 已连接
     */
    if (pressure_1 < 20.0f) {
        cooling_ui_set_tank_connection(TANK_DISCONNECTED);
    } else {
        cooling_ui_set_tank_connection(TANK_CONNECTED);
    }

    /* 6. 超压告警逻辑
     * pressure_1 > 130 PSI 或 pressure_2 > 130 PSI 时显示警告
     * 否则隐藏警告
     */
    if ((pressure_1 > 130.0f) || (pressure_2 > 130.0f)) {
        cooling_ui_show_overpressure_warning();
    } else {
        cooling_ui_hide_overpressure_warning();
    }

//    /* 7. 计算 PID */
    /* PID closed loop: update TIM8 PWM compare values. */
    float pwm_out_1 = PIDController_Update(&pid_ch1, target_pressure_ch1, pressure_1);
    float pwm_out_2 = PIDController_Update(&pid_ch2, target_pressure_ch2, pressure_2);

    if (pwm_out_1 < 0.0f) {
        pwm_out_1 = 0.0f;
    } else if (pwm_out_1 > 10000.0f) {
        pwm_out_1 = 10000.0f;
    }

    if (pwm_out_2 < 0.0f) {
        pwm_out_2 = 0.0f;
    } else if (pwm_out_2 > 10000.0f) {
        pwm_out_2 = 10000.0f;
    }

    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, (uint32_t)pwm_out_1);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, (uint32_t)pwm_out_2);


//    /* 8. 对 PWM 输出做限幅，避免越界 */

//    /* 9. 更新 TIM8 PWM (ARR = 10000) */
		
		HAL_GPIO_TogglePin(LED1_GPIO_Port,LED1_Pin);//debug用，不用可删除
		
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
    
    /* 2. 初始化 PID 1 */
    PIDController_Init(&pid_ch1);
    pid_ch1.Kp = 1.0f;      // 【需用户整定】
    pid_ch1.Ki = 0.5f;      // 【需用户整定】
    pid_ch1.Kd = 0.0f;      // 【需用户整定】
    pid_ch1.tau = 0.02f;    // 导数低通滤波时间常数
    pid_ch1.limMin = 0.0f;          // PWM 下限
    pid_ch1.limMax = 10000.0f;      // PWM 上限 (对应 TIM8 ARR)
    pid_ch1.limMinInt = -5000.0f;   // 积分下限抗饱和
    pid_ch1.limMaxInt = 5000.0f;    // 积分上限抗饱和
    pid_ch1.T = 0.5f;       // 采样时间为 500ms = 0.5s

    /* 3. 初始化 PID 2 (参数类似) */
    PIDController_Init(&pid_ch2);
    pid_ch2.Kp = 1.0f;
    pid_ch2.Ki = 0.5f;
    pid_ch2.Kd = 0.0f;
    pid_ch2.tau = 0.02f;
    pid_ch2.limMin = 0.0f;
    pid_ch2.limMax = 10000.0f;
    pid_ch2.limMinInt = -5000.0f;
    pid_ch2.limMaxInt = 5000.0f;
    pid_ch2.T = 0.5f;

    /* 4. 注册事件回调 */
    evt_register_handler(APP_EVT_ADC_PID, on_adc_pid_event);
    evt_register_handler(APP_EVT_LVGL_TICK, on_lvgl_tick_event);
    evt_register_handler(APP_EVT_VALVE_CHECK, on_valve_check_event);
    
    /* 启动 PWM  */
		HAL_StatusTypeDef rtv_ch1,rtv_ch2;
		HAL_TIM_Base_Start_IT(&htim11);
		HAL_TIM_Base_Start_IT(&htim13);
		HAL_TIM_Base_Start_IT(&htim14);
    rtv_ch1 = HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);
    rtv_ch2 = HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2);
		
		if(rtv_ch1 == HAL_OK && rtv_ch2 == HAL_OK){
			
			cooling_ui_set_heater_state(HEATER_ON);
			
		}else if(rtv_ch1 == HAL_ERROR || rtv_ch2 == HAL_ERROR){
			
			cooling_ui_set_heater_state(HEATER_ERROR);

		}else{
			
			cooling_ui_set_heater_state(HEATER_OFF);
		
		}
			
}

/* 以下函数请在 stm32xxx_it.c 中的对应定时器中断服务函数中调用 */

void App_ISR_TIM11_500ms(void) {
    // 发布 ADC/PID 事件 (参数和指针都传 0/NULL 即可)
    evt_publish(APP_EVT_ADC_PID, 0, NULL);
}

void App_ISR_TIM13_5ms(void) {
    // 关键：时基的心跳必须在中断里立刻增加
    lv_tick_inc(5); 
    // 发布调度事件让主循环执行 lv_task_handler()
    evt_publish(APP_EVT_LVGL_TICK, 0, NULL);
}

void App_ISR_TIM14_1s(void) {
    evt_publish(APP_EVT_VALVE_CHECK, 0, NULL);
	
}
