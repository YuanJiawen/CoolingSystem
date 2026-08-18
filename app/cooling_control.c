/**
 * @file    cooling_control.c
 * @brief   冷却控制模块实现(纯计算,零硬件依赖)
 */

#include "cooling_control.h"

float CoolingControl_PressureFromAdc(uint32_t adc_val)
{
    /* 1. 计算电压 (12 位 ADC, Vref=3V) */
    float voltage = ((float)(adc_val * 2) / 4095.0f) * 3.0f;

    /* 2. 计算压力 (V - 0.5) / 0.0266 */
    float pressure = (voltage - 0.5f) / 0.0266f;

    /* 3. 负压截断保护 (滤除零点底噪) */
    if (pressure < 0.0f) {
        pressure = 0.0f;
    }
    return pressure;
}

uint8_t CoolingControl_ValveEnable(const CoolingCtrlConfig *cfg, float pipe_pressure)
{
    return (pipe_pressure > cfg->ctrl_enable_psi) ? 1U : 0U;
}

uint8_t CoolingControl_IsTankConnected(const CoolingCtrlConfig *cfg, float pressure)
{
    return (pressure >= cfg->tank_connected_psi) ? 1U : 0U;
}

uint8_t CoolingControl_IsOverpressure(const CoolingCtrlConfig *cfg,
                                      float pressure_1, float pressure_2)
{
    return ((pressure_1 > cfg->overpressure_psi) ||
            (pressure_2 > cfg->overpressure_psi)) ? 1U : 0U;
}

float CoolingControl_DisplayPressure(float psi)
{
    if (psi < 1.0f) {
        return 0.0f; /* 零点死区:滤除 <1 PSI 底噪 */
    }
    if (psi > 150.0f) {
        return 150.0f; /* 显示量程上限 */
    }
    return psi;
}
