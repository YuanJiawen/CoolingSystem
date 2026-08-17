/**
 * @file    pressure_sampler.c
 * @brief   压力采样适配器实现(纯硬件 adapter,ISR 上下文)
 */
#include "pressure_sampler.h"

/* ========================== 单例状态 ========================== */

static ADC_HandleTypeDef       *s_hadc;
static const CoolingCtrlConfig *s_cfg;
static GPIO_TypeDef            *s_alarm_port;
static uint16_t                 s_alarm_pin;

/* 进行中缓冲(ISR 写)与已发布快照(宿主读)分离,消除混读竞态 */
static float   s_working[2];   /* 本轮进行中的两通道压力 */
static uint8_t s_chan_phase;   /* 0=通道1, 1=通道2 */
static uint8_t s_busy;         /* 1=本轮采样进行中 */

static PressureSnapshot s_current; /* valid=0 表示尚无完整快照 */

/* ========================== 内部工具 ========================== */

/**
 * @brief 配置并启动单通道中断采样;失败则结束本轮(宿主下个周期自动重试)
 */
static void sampler_start_channel(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    sConfig.Channel = channel;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;

    if (HAL_ADC_ConfigChannel(s_hadc, &sConfig) != HAL_OK) {
        s_busy = 0; /* 配置失败,结束本轮,避免采样冻结 */
        return;
    }
    if (HAL_ADC_Start_IT(s_hadc) != HAL_OK) {
        s_busy = 0; /* 启动失败,结束本轮 */
    }
}

/* ========================== API 实现 ========================== */

void PressureSampler_Init(ADC_HandleTypeDef *hadc, const CoolingCtrlConfig *cfg,
                          GPIO_TypeDef *alarm_port, uint16_t alarm_pin)
{
    s_hadc       = hadc;
    s_cfg        = cfg;
    s_alarm_port = alarm_port;
    s_alarm_pin  = alarm_pin;

    s_busy       = 0;
    s_chan_phase = 0;
    s_working[0] = 0.0f;
    s_working[1] = 0.0f;

    s_current.pressure[0]  = 0.0f;
    s_current.pressure[1]  = 0.0f;
    s_current.valid        = 0;
    s_current.overpressure = 0;
}

void PressureSampler_Start(void)
{
    if (s_busy) {
        return; /* 幂等:上一轮未完成时不重入 */
    }
    s_busy       = 1;
    s_chan_phase = 0;
    sampler_start_channel(ADC_CHANNEL_1);
}

uint8_t PressureSampler_IsBusy(void)
{
    return s_busy;
}

void PressureSampler_GetSnapshot(PressureSnapshot *out)
{
    if (out != NULL) {
        *out = s_current;
    }
}

/* ========================== ISR 回调 ========================== */

/**
 * @brief ADC 转换完成中断回调(超压快路径核心,ISR 上下文)
 * @note  UI 更新不在此处做(LVGL 非中断安全),由主循环经快照同步。
 *        超压阈值取自 cfg(单源),不再读裸宏。
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc != s_hadc) {
        return; /* 只处理自己管理的 ADC */
    }

    uint32_t raw = HAL_ADC_GetValue(hadc);
    float psi = CoolingControl_PressureFromAdc(raw);

    if (s_chan_phase == 0) {
        /* 通道1 完成:立即置位(若超压),然后启动通道2 */
        s_working[0] = psi;
        if (psi > s_cfg->overpressure_psi) {
            HAL_GPIO_WritePin(s_alarm_port, s_alarm_pin, GPIO_PIN_SET);
        }
        s_chan_phase = 1;
        sampler_start_channel(ADC_CHANNEL_2);
    } else {
        /* 通道2 完成:综合两通道判定(可清除告警),一轮结束并发布快照 */
        s_working[1] = psi;
        uint8_t over = (s_working[0] > s_cfg->overpressure_psi) ||
                       (s_working[1] > s_cfg->overpressure_psi);
        if (over) {
            HAL_GPIO_WritePin(s_alarm_port, s_alarm_pin, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(s_alarm_port, s_alarm_pin, GPIO_PIN_RESET);
        }

        /* 双缓冲发布:一次性锁存,GetSnapshot 永不见半新半旧 */
        s_current.pressure[0]  = s_working[0];
        s_current.pressure[1]  = s_working[1];
        s_current.overpressure = over;
        s_current.valid        = 1;

        s_busy = 0;
    }
}
