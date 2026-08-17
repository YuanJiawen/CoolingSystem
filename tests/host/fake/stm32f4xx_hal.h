/**
 * @file    fake/stm32f4xx_hal.h
 * @brief   SDIO 非致命化回归测试用 HAL 桩头文件(host 专用,非硬件代码)
 *
 * 仅提供 sdio.c 编译所需的类型/宏/函数声明;具体行为由
 * test_sdio_nofatal.c 中的测试替身实现。
 */
#ifndef FAKE_STM32F4XX_HAL_H
#define FAKE_STM32F4XX_HAL_H

#include <stdint.h>

/* ---------- HAL 状态 ---------- */
typedef int32_t HAL_StatusTypeDef;
#define HAL_OK        0
#define HAL_ERROR     1
#define HAL_BUSY      2
#define HAL_TIMEOUT   3

typedef enum {
    HAL_SD_STATE_RESET = 0,
    HAL_SD_STATE_READY = 1,
    HAL_SD_STATE_BUSY  = 2
} HAL_SD_StateTypeDef;

/* ---------- 外设结构(仅编译所需字段) ---------- */
typedef struct { int dummy; } SDIO_TypeDef;
typedef struct { int dummy; } GPIO_TypeDef;
typedef struct { int dummy; } DMA_Stream_TypeDef;

typedef struct {
    uint32_t Pin, Mode, Pull, Speed, Alternate;
} GPIO_InitTypeDef;

typedef struct {
    uint32_t ClockEdge, ClockBypass, ClockPowerSave, BusWide,
             HardwareFlowControl, ClockDiv;
} SD_InitTypeDef;

typedef struct {
    DMA_Stream_TypeDef *Instance;
    uint32_t Channel, Direction, PeriphInc, MemInc,
             PeriphDataAlignment, MemDataAlignment, Mode, Priority,
             FIFOMode, FIFOThreshold, MemBurst, PeriphBurst;
} DMA_InitTypeDef;

typedef struct {
    DMA_Stream_TypeDef *Instance;
    DMA_InitTypeDef     Init;
    void               *Parent;
} DMA_HandleTypeDef;

typedef struct {
    SDIO_TypeDef       *Instance;
    SD_InitTypeDef      Init;
    HAL_SD_StateTypeDef State;
    uint32_t            ErrorCode;
    DMA_HandleTypeDef  *hdmarx;
    DMA_HandleTypeDef  *hdmatx;
} SD_HandleTypeDef;

/* ---------- 实例基址宏 ---------- */
#define SDIO  ((SDIO_TypeDef *)0x40012C00UL)
#define GPIOC ((GPIO_TypeDef *)0x40020800UL)
#define GPIOD ((GPIO_TypeDef *)0x40020C00UL)
#define DMA2_Stream3 ((DMA_Stream_TypeDef *)0x40026440UL)
#define DMA2_Stream6 ((DMA_Stream_TypeDef *)0x400264C0UL)

/* ---------- SDIO Init 选项值 ---------- */
#define SDIO_CLOCK_EDGE_RISING           0U
#define SDIO_CLOCK_BYPASS_DISABLE        0U
#define SDIO_CLOCK_POWER_SAVE_ENABLE     2U
#define SDIO_BUS_WIDE_1B                 0U
#define SDIO_HARDWARE_FLOW_CONTROL_ENABLE 1U
#define SDIO_BUS_WIDE_4B                 1U

/* ---------- GPIO ---------- */
#define GPIO_PIN_2   0x0002U
#define GPIO_PIN_8   0x0100U
#define GPIO_PIN_9   0x0200U
#define GPIO_PIN_10  0x0400U
#define GPIO_PIN_11  0x0800U
#define GPIO_PIN_12  0x1000U
#define GPIO_MODE_AF_PP          2U
#define GPIO_PULLUP              1U
#define GPIO_SPEED_FREQ_VERY_HIGH 3U
#define GPIO_AF12_SDIO          12U

/* ---------- DMA ---------- */
#define DMA_CHANNEL_4          4U
#define DMA_PERIPH_TO_MEMORY   0U
#define DMA_MEMORY_TO_PERIPH   1U
#define DMA_PINC_DISABLE       0U
#define DMA_MINC_ENABLE        1U
#define DMA_PDATAALIGN_WORD    2U
#define DMA_MDATAALIGN_WORD    2U
#define DMA_PFCTRL             2U
#define DMA_PRIORITY_VERY_HIGH 3U
#define DMA_FIFOMODE_ENABLE    1U
#define DMA_FIFO_THRESHOLD_FULL 3U
#define DMA_MBURST_INC4        3U
#define DMA_PBURST_INC4        3U

/* ---------- NVIC ---------- */
#define SDIO_IRQn 49

/* ---------- RCC 使能宏 ---------- */
#define __HAL_RCC_SDIO_CLK_ENABLE()   do { } while (0)
#define __HAL_RCC_SDIO_CLK_DISABLE()  do { } while (0)
#define __HAL_RCC_GPIOC_CLK_ENABLE()  do { } while (0)
#define __HAL_RCC_GPIOD_CLK_ENABLE()  do { } while (0)

/* ---------- 链接宏 ---------- */
#define __HAL_LINKDMA(HANDLE, FIELD, DMAHANDLE) \
    do { (HANDLE)->FIELD = &(DMAHANDLE); (DMAHANDLE).Parent = (HANDLE); } while (0)

/* ---------- HAL API(由测试替身实现) ---------- */
HAL_StatusTypeDef HAL_SD_Init(SD_HandleTypeDef *hsd);
HAL_StatusTypeDef HAL_SD_ConfigWideBusOperation(SD_HandleTypeDef *hsd, uint32_t WideMode);
HAL_StatusTypeDef HAL_SD_DeInit(SD_HandleTypeDef *hsd);
void HAL_SD_MspInit(SD_HandleTypeDef *hsd);
void HAL_SD_MspDeInit(SD_HandleTypeDef *hsd);
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef *hdma);
HAL_StatusTypeDef HAL_DMA_DeInit(DMA_HandleTypeDef *hdma);
void HAL_GPIO_Init(GPIO_TypeDef *GPIOx, GPIO_InitTypeDef *GPIO_Init);
void HAL_GPIO_DeInit(GPIO_TypeDef *GPIOx, uint32_t GPIO_Pin);
void HAL_NVIC_SetPriority(uint32_t IRQn, uint32_t PreemptPriority, uint32_t SubPriority);
void HAL_NVIC_EnableIRQ(uint32_t IRQn);
void HAL_NVIC_DisableIRQ(uint32_t IRQn);

#endif /* FAKE_STM32F4XX_HAL_H */
