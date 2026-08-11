/**
 * @file    cooling_control.c
 * @brief   冷却控制模块实现(纯计算,零硬件依赖)
 */

#include "cooling_control.h"

/* ========================== 内部工具 ========================== */

static float clampf(float val, float lo, float hi)
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

/* ========================== API 实现 ========================== */

void CoolingControl_Init(CoolingCtrl *ctrl, const CoolingCtrlConfig *cfg)
{
    PIDController_Init(&ctrl->pid);

    ctrl->pid.Kp        = cfg->kp;
    ctrl->pid.Ki        = cfg->ki;
    ctrl->pid.Kd        = cfg->kd;
    ctrl->pid.tau       = cfg->tau;
    ctrl->pid.limMin    = cfg->lim_min;
    ctrl->pid.limMax    = cfg->lim_max;
    ctrl->pid.limMinInt = cfg->lim_min_int;
    ctrl->pid.limMaxInt = cfg->lim_max_int;
    ctrl->pid.T         = cfg->sample_time_s;
}

CoolingCtrlOut CoolingControl_Step(CoolingCtrl *ctrl, const CoolingCtrlConfig *cfg,
                                   float setpoint, float pressure)
{
    CoolingCtrlOut out;

    /* 1. 状态判定(纯逻辑,可 host 测试) */
    out.tank_connected = (pressure >= cfg->tank_connected_psi) ? 1U : 0U;
    out.overpressure   = (pressure >  cfg->overpressure_psi)  ? 1U : 0U;

    /* 2. PID 闭环:更新控制量(内部已按 limMin/limMax 限幅) */
    out.pwm = PIDController_Update(&ctrl->pid, setpoint, pressure);

    /* 3. 输出限幅(双保险:即使 PID 内部限幅被绕过也保证不越界) */
    out.pwm = clampf(out.pwm, cfg->lim_min, cfg->lim_max);

    return out;
}

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

uint8_t CoolingControl_ValidateConfig(const CoolingCtrlConfig *cfg,
                                      uint32_t pwm_arr,
                                      uint32_t sample_period_ms)
{
    /* PWM 输出上限必须等于定时器 ARR+1(占空比尺度一致) */
    if ((uint32_t)cfg->lim_max != (pwm_arr + 1U)) {
        return 0;
    }
    /* 控制周期必须等于采样定时器中断周期 */
    if ((uint32_t)(cfg->sample_time_s * 1000.0f) != sample_period_ms) {
        return 0;
    }
    return 1;
}
