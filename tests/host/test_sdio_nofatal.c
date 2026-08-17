/**
 * @file    test_sdio_nofatal.c
 * @brief   无 SD 卡不阻塞开机 —— MX_SDIO_SD_Init 非致命化的回归测试
 *
 * 症状(用户报告):设备在无 SD 卡时无法开机。
 * 根因:修复前 MX_SDIO_SD_Init 在 HAL_SD_Init/HAL_SD_ConfigWideBusOperation
 *       失败时调用 Error_Handler()(关中断死循环),无卡必触发。
 * 期望行为:失败时不挂死、句柄复位为 RESET(供 f_mount -> disk_initialize
 *       重新完整初始化)、返回状态码。
 *
 * 构建/运行:sh run_sdio_nofatal.sh [void]
 *   - 默认(修复后代码):应全部 PASS
 *   - void 参数(修复前旧代码):场景 A/C 必须 FAIL(红验证)
 *
 * 该测试为 freestanding(无 libc),由 clang 编为 wasm 后经 node 执行,
 * 失败数由 run_tests() 返回,首个失败行号经 g_last_fail_line 导出。
 */

#include "sdio.h"

/* ==================== 测试替身(记录调用,替代真实 HAL) ==================== */

static int g_error_handler_called = 0;
static int g_sd_init_calls      = 0;
static int g_widebus_calls      = 0;
static int g_sd_deinit_calls    = 0;
static HAL_StatusTypeDef g_sd_init_ret = HAL_OK;
static HAL_StatusTypeDef g_widebus_ret = HAL_OK;

/* 首个失败断言行号(wasm 全局导出,node 侧读取) */
int g_last_fail_line = 0;

void Error_Handler(void)
{
    g_error_handler_called = 1;
}

HAL_StatusTypeDef HAL_SD_Init(SD_HandleTypeDef *hsd)
{
    g_sd_init_calls++;
    HAL_SD_MspInit(hsd);
    /* 复刻 F4 HAL 行为:即使初始化失败,State 也常被置为 READY(quirk),
     * 这正是修复必须在失败路径调用 HAL_SD_DeInit 复位的原因。 */
    hsd->State = HAL_SD_STATE_READY;
    return g_sd_init_ret;
}

HAL_StatusTypeDef HAL_SD_ConfigWideBusOperation(SD_HandleTypeDef *hsd, uint32_t WideMode)
{
    (void)hsd;
    (void)WideMode;
    g_widebus_calls++;
    return g_widebus_ret;
}

HAL_StatusTypeDef HAL_SD_DeInit(SD_HandleTypeDef *hsd)
{
    g_sd_deinit_calls++;
    HAL_SD_MspDeInit(hsd);
    hsd->State = HAL_SD_STATE_RESET;
    return HAL_OK;
}

/* ---------- 其余 HAL 依赖为无操作桩 ---------- */
void HAL_GPIO_Init(GPIO_TypeDef *GPIOx, GPIO_InitTypeDef *GPIO_Init)
{
    (void)GPIOx; (void)GPIO_Init;
}

void HAL_GPIO_DeInit(GPIO_TypeDef *GPIOx, uint32_t GPIO_Pin)
{
    (void)GPIOx; (void)GPIO_Pin;
}

HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef *hdma)
{
    (void)hdma;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_DMA_DeInit(DMA_HandleTypeDef *hdma)
{
    (void)hdma;
    return HAL_OK;
}

void HAL_NVIC_SetPriority(uint32_t IRQn, uint32_t PreemptPriority, uint32_t SubPriority)
{
    (void)IRQn; (void)PreemptPriority; (void)SubPriority;
}

void HAL_NVIC_EnableIRQ(uint32_t IRQn)  { (void)IRQn; }
void HAL_NVIC_DisableIRQ(uint32_t IRQn) { (void)IRQn; }

/* ==================== 测试主体 ==================== */

static int g_failures = 0;

#define CHECK(cond, line) \
    do { if (!(cond)) { g_failures++; g_last_fail_line = (line); } } while (0)

static void reset_observers(void)
{
    g_error_handler_called = 0;
    g_sd_init_calls      = 0;
    g_widebus_calls      = 0;
    g_sd_deinit_calls    = 0;
    g_sd_init_ret        = HAL_OK;
    g_widebus_ret        = HAL_OK;
    hsd.State            = HAL_SD_STATE_RESET;
}

int run_tests(void)
{
    g_failures = 0;

    /* ---- 场景 A:无卡(HAL_SD_Init 失败)——用户报告的症状 ---- */
    reset_observers();
    g_sd_init_ret = HAL_ERROR;
#ifdef SDIO_VOID_INIT
    MX_SDIO_SD_Init();
    CHECK(g_error_handler_called == 0, __LINE__);   /* 不得进入 Error_Handler 死循环 */
#else
    CHECK(MX_SDIO_SD_Init() == 0U, __LINE__);        /* 返回失败状态码 */
    CHECK(g_error_handler_called == 0, __LINE__);   /* 不得进入 Error_Handler 死循环 */
#endif
    CHECK(g_sd_deinit_calls == 1, __LINE__);        /* 句柄被复位,供重试 */
    CHECK(hsd.State == HAL_SD_STATE_RESET, __LINE__); /* 重试入口状态正确 */

    /* ---- 场景 B:卡在位,初始化全部成功 ---- */
    reset_observers();
#ifdef SDIO_VOID_INIT
    MX_SDIO_SD_Init();
    CHECK(g_sd_init_calls == 1 && g_widebus_calls == 1, __LINE__);
#else
    CHECK(MX_SDIO_SD_Init() == 1U, __LINE__);        /* 返回成功 */
    CHECK(g_sd_init_calls == 1 && g_widebus_calls == 1, __LINE__);
#endif
    CHECK(g_error_handler_called == 0, __LINE__);
    CHECK(g_sd_deinit_calls == 0, __LINE__);        /* 成功路径不应 DeInit */
    CHECK(hsd.State == HAL_SD_STATE_READY, __LINE__);

    /* ---- 场景 C:初始化成功但 4-bit 总线协商失败 ---- */
    reset_observers();
    g_widebus_ret = HAL_ERROR;
#ifdef SDIO_VOID_INIT
    MX_SDIO_SD_Init();
    CHECK(g_error_handler_called == 0, __LINE__);
#else
    CHECK(MX_SDIO_SD_Init() == 0U, __LINE__);
    CHECK(g_error_handler_called == 0, __LINE__);
#endif
    CHECK(g_sd_deinit_calls == 1, __LINE__);
    CHECK(hsd.State == HAL_SD_STATE_RESET, __LINE__);

    return g_failures;
}
