/**
 * @file    test_pressure_sampler.c
 * @brief   压力采样适配器 pressure_sampler 的 host 回归测试(fake HAL)
 *
 * 覆盖:
 *   1. 上电首拍无快照(valid=0)
 *   2. 完整一轮两通道采样 → 快照正确、busy 清零、无超压 GPIO 复位
 *   3. 超压快路径(链接真执行器):ch1 完成即锁存 —— 加热片两通道 compare
 *      归零 + ALARM 置位;ch2 综合锁存 overpressure
 *   4. Start 幂等(busy 时重入无效)
 *   5. 竞态回归:新一轮仅完成 ch1 时,GetSnapshot 仍返回上一完整轮
 *      (验证双缓冲,杜绝「新 ch1 + 旧 ch2」混读)
 *   6. 临界区配平(fake PRIMASK):Start 置位 / ch2 发布 / GetSnapshot
 *      拷贝均发生在关中断区内,且每次调用后保存/恢复配平(不漏关中断)
 *
 * 构建/运行:sh run_pressure_sampler.sh(clang wasm + node)
 */

#include "pressure_sampler.h"
#include "cooling_control_config.h"
#include "cooling_actuator.h"

extern void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);
/* ==================== 测试替身(记录 HAL 调用) ==================== */

static uint32_t g_configured_channel = 0;
static uint32_t g_raw_ch1 = 0;   /* 测试注入的 ch1 原始 ADC 值 */
static uint32_t g_raw_ch2 = 0;   /* 测试注入的 ch2 原始 ADC 值 */
static uint8_t  g_gpio_state = 0;   /* 0=RESET, 1=SET */
static int g_start_it_calls = 0;
static int g_config_calls = 0;
static uint32_t g_compare[2] = {0xFFFFU, 0xFFFFU}; /* 各通道最近 compare(执行器观察) */

static ADC_HandleTypeDef g_fake_hadc;
static TIM_HandleTypeDef g_fake_htim;

HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef *hadc, ADC_ChannelConfTypeDef *sConfig)
{
    (void)hadc;
    g_config_calls++;
    g_configured_channel = sConfig->Channel;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_ADC_Start_IT(ADC_HandleTypeDef *hadc)
{
    (void)hadc;
    g_start_it_calls++;
    return HAL_OK;
}

uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *hadc)
{
    (void)hadc;
    return (g_configured_channel == ADC_CHANNEL_1) ? g_raw_ch1 : g_raw_ch2;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState)
{
    (void)GPIOx;
    (void)GPIO_Pin;
    g_gpio_state = (PinState == GPIO_PIN_SET) ? 1U : 0U;
}

void fake_tim_set_compare(TIM_HandleTypeDef *htim, uint32_t channel, uint32_t value)
{
    (void)htim;
    g_compare[channel] = value;
}

HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *htim, uint32_t Channel)
{
    (void)htim;
    (void)Channel;
    return HAL_OK;
}

/* ==================== 测试主体 ==================== */

static int g_failures = 0;
int g_last_fail_line = 0;
#include "check.h"

/* ==================== fake PRIMASK(临界区仪器化) ==================== */

static uint32_t g_primask      = 0;  /* 0=开中断,1=关中断(初始开) */
static int      g_disable_hits = 0;  /* __disable_irq 调用次数 */

uint32_t fake_get_primask(void) { return g_primask; }
void     fake_disable_irq(void) { g_primask = 1u; g_disable_hits++; }
void     fake_set_primask(uint32_t m) { g_primask = m; }

static void reset_observers(void)
{
    g_configured_channel = 0;
    g_raw_ch1 = 0;
    g_raw_ch2 = 0;
    g_gpio_state = 0;
    g_start_it_calls = 0;
    g_config_calls = 0;
    g_compare[0] = 0xFFFFU;
    g_compare[1] = 0xFFFFU;
}

int run_tests(void)
{
    g_failures = 0;
    const CoolingCtrlConfig *cfg = &cooling_control_default_config;
    PressureSnapshot snap;

    /* ---- 场景 1:上电首拍无快照 ---- */
    CoolingActuator_Init(&g_fake_htim, (GPIO_TypeDef *)0, 0, (GPIO_TypeDef *)0, 0);
    PressureSampler_Init(&g_fake_hadc, cfg);
    PressureSampler_GetSnapshot(&snap);
    CHECK(snap.valid == 0, __LINE__);

    /* ---- 场景 2:完整一轮两通道采样 ---- */
    reset_observers();
    g_raw_ch1 = 683;
    g_raw_ch2 = 1365;
    float exp1 = CoolingControl_PressureFromAdc(683);
    float exp2 = CoolingControl_PressureFromAdc(1365);
    PressureSampler_Start();
    CHECK(PressureSampler_IsBusy() == 1, __LINE__);
    HAL_ADC_ConvCpltCallback(&g_fake_hadc);   /* ch1 */
    HAL_ADC_ConvCpltCallback(&g_fake_hadc);   /* ch2 */
    CHECK(PressureSampler_IsBusy() == 0, __LINE__);
    PressureSampler_GetSnapshot(&snap);
    CHECK(snap.valid == 1, __LINE__);
    CHECK_NEAR(snap.pressure[0], exp1, 1e-3f, __LINE__);
    CHECK_NEAR(snap.pressure[1], exp2, 1e-3f, __LINE__);
    CHECK(snap.overpressure == 0, __LINE__);
    CHECK(g_gpio_state == 0, __LINE__);        /* 无超压 → GPIO 复位 */

    /* ---- 场景 3:超压快路径(真执行器链) ---- */
    reset_observers();
    CoolingActuator_HeaterSetDuty(2000U, 2000U);   /* 预置非零占空比 */
    g_raw_ch1 = 3000;   /* 远超阈值(>130 PSI) */
    g_raw_ch2 = 683;
    PressureSampler_Start();
    HAL_ADC_ConvCpltCallback(&g_fake_hadc);   /* ch1 完成即锁存 */
    CHECK(g_compare[0] == 0, __LINE__);       /* 加热片 ch1 立即归零 */
    CHECK(g_compare[1] == 0, __LINE__);       /* 加热片 ch2 立即归零 */
    CHECK(g_gpio_state == 1, __LINE__);       /* ALARM 置位 */
    HAL_ADC_ConvCpltCallback(&g_fake_hadc);   /* ch2 完成综合判定 */
    PressureSampler_GetSnapshot(&snap);
    CHECK(snap.overpressure == 1, __LINE__);
    CHECK(g_gpio_state == 1, __LINE__);

    /* ---- 场景 4:Start 幂等 ---- */
    reset_observers();
    PressureSampler_Start();
    int calls = g_start_it_calls;
    PressureSampler_Start();                   /* busy → 不重入 */
    CHECK(g_start_it_calls == calls, __LINE__);
    HAL_ADC_ConvCpltCallback(&g_fake_hadc);   /* 收尾:完成 ch1/ch2,回到空闲 */
    HAL_ADC_ConvCpltCallback(&g_fake_hadc);

    /* ---- 场景 5:竞态回归(双缓冲) ---- */
    reset_observers();
    g_raw_ch1 = 683;
    g_raw_ch2 = 1365;
    PressureSampler_Start();
    HAL_ADC_ConvCpltCallback(&g_fake_hadc);   /* 完整轮 A 发布 */
    HAL_ADC_ConvCpltCallback(&g_fake_hadc);

    g_raw_ch1 = 3000;   /* 试图用新一轮污染 */
    g_raw_ch2 = 9999;
    PressureSampler_Start();
    HAL_ADC_ConvCpltCallback(&g_fake_hadc);   /* 仅完成 ch1,ch2 未完 */

    PressureSampler_GetSnapshot(&snap);
    CHECK(snap.valid == 1, __LINE__);
    CHECK_NEAR(snap.pressure[0], exp1, 1e-3f, __LINE__);   /* 仍是轮 A 的 ch1 */
    CHECK_NEAR(snap.pressure[1], exp2, 1e-3f, __LINE__);   /* 仍是轮 A 的 ch2 */

    /* ---- 场景 6:临界区配平(fake PRIMASK 仪器化) ---- */
    reset_observers();
    g_primask      = 0;
    g_disable_hits = 0;
    PressureSampler_Init(&g_fake_hadc, cfg);

    int hits = g_disable_hits;
    PressureSampler_Start();                 /* busy 置位须在临界区内 */
    CHECK(g_disable_hits > hits, __LINE__);
    CHECK(g_primask == 0, __LINE__);         /* 退出后配平:恢复开中断 */

    HAL_ADC_ConvCpltCallback(&g_fake_hadc);  /* ch1(无共享发布,不强制) */
    CHECK(g_primask == 0, __LINE__);

    hits = g_disable_hits;
    HAL_ADC_ConvCpltCallback(&g_fake_hadc);  /* ch2:快照发布须在临界区内 */
    CHECK(g_disable_hits > hits, __LINE__);
    CHECK(g_primask == 0, __LINE__);

    hits = g_disable_hits;
    PressureSampler_GetSnapshot(&snap);      /* 12 字节拷贝须在临界区内 */
    CHECK(g_disable_hits > hits, __LINE__);
    CHECK(g_primask == 0, __LINE__);
    CHECK(snap.valid == 1, __LINE__);

    return g_failures;
}
