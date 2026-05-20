#if 0
/*********************
 * INCLUDES
 *********************/
#include "lv_port_disp_template.h"
#include <stdbool.h>
#include <string.h>
#include "bsp_sdram.h"
#include "stm32f4xx_hal.h"

/*********************
 * DEFINES
 *********************/
#ifndef MY_DISP_HOR_RES
    #define MY_DISP_HOR_RES    800
#endif
#ifndef MY_DISP_VER_RES
    #define MY_DISP_VER_RES    480
#endif

/* 全屏双缓冲模式：每个缓冲区就是一整帧的大小 */
#define FRAME_BUFFER_SIZE    (MY_DISP_HOR_RES * MY_DISP_VER_RES * sizeof(lv_color_t))

/* 显存地址 (分配在外部 SDRAM 中) */
#define LCD_FRAME_BUF_ADDR1  (SDRAM_BANK_ADDR)
#define LCD_FRAME_BUF_ADDR2  (SDRAM_BANK_ADDR + FRAME_BUFFER_SIZE)

/**********************
 * EXTERN HANDLES
 **********************/
extern LTDC_HandleTypeDef  hltdc;
extern SDRAM_HandleTypeDef hsdram2;

/**********************
 * STATIC PROTOTYPES
 **********************/
static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);

/**********************
 * STATIC VARIABLES
 **********************/
static volatile lv_disp_drv_t *s_disp_drv = NULL;

/**********************
 * GLOBAL FUNCTIONS
 **********************/
/**
 * @brief  初始化显示驱动并注册到 LVGL (采用全屏双显存指针翻转架构)
 */
void lv_port_disp_init(void)
{
    /* 1. 硬件初始化 */
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_7, GPIO_PIN_SET);
    
    /* 初始时，让 LTDC 指向缓冲区 1 */
    HAL_LTDC_SetAddress(&hltdc, LCD_FRAME_BUF_ADDR1, 0);

    /* 2. 声明并初始化全屏绘制缓冲区 */
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t *buf_1 = (lv_color_t *)LCD_FRAME_BUF_ADDR1;
    static lv_color_t *buf_2 = (lv_color_t *)LCD_FRAME_BUF_ADDR2;
    
    /* 注意：此处传入的长度是全屏像素总数，而不是之前的 DRAW_BUF_LINES */
    lv_disp_draw_buf_init(&draw_buf, buf_1, buf_2, MY_DISP_HOR_RES * MY_DISP_VER_RES);

    /* 3. 注册显示驱动 */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    
    disp_drv.hor_res      = MY_DISP_HOR_RES;
    disp_drv.ver_res      = MY_DISP_VER_RES;
    disp_drv.flush_cb     = disp_flush;
    disp_drv.draw_buf     = &draw_buf;
    
    /* 采用代码 B 逻辑：开启全屏强制刷新配合指针翻转 */
    disp_drv.full_refresh = 1; 

    lv_disp_drv_register(&disp_drv);
    
    /* 4. 配置 LTDC 行中断 (对应代码 B 的中断逻辑) */
    /* 在第 0 行产生中断（即垂直消隐期结束，开始扫描新一帧时） */
    HAL_LTDC_ProgramLineEvent(&hltdc, 0);
    __HAL_LTDC_ENABLE_IT(&hltdc, LTDC_IT_LI);
    
    /* 确保在 stm32f4xx_it.c 中有 LTDC_IRQHandler 调用 HAL_LTDC_IRQHandler */
    HAL_NVIC_SetPriority(LTDC_IRQn, 14, 0);
    HAL_NVIC_EnableIRQ(LTDC_IRQn);
}

/**********************
 * FLUSH CALLBACK
 **********************/
/**
 * @brief  LVGL flush 回调 —— 切换 LTDC 显存地址
 */
static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    /* 保存驱动句柄，供中断回调使用以解除阻塞 */
    s_disp_drv = disp_drv;

    /* 采用代码 B 逻辑：直接通过寄存器切换 LTDC Layer 1 的显存基地址 */
    LTDC_Layer1->CFBAR = (uint32_t)color_p;
    
    /* 触发垂直空白期重载 (VBR)，使新的基地址在下一帧扫描时生效 */
    LTDC->SRCR |= LTDC_SRCR_VBR;
}

/**
 * @brief  LTDC 行事件完成回调 (对应代码 B 的 HAL_LTDC_LineEvenCallback)
 * @note   需要在 stm32f4xx_it.c 的 LTDC_IRQHandler 中调用 HAL_LTDC_IRQHandler
 */
void HAL_LTDC_LineEventCallback(LTDC_HandleTypeDef *hltdc)
{
    /* HAL 库要求每次行中断触发后，必须重新配置才能在下一帧再次触发 */
    HAL_LTDC_ProgramLineEvent(hltdc, 0); 
    
    /* 如果存在未完成的刷新请求，通知 LVGL 刷新已完成 */
    if (s_disp_drv) {
        lv_disp_flush_ready((lv_disp_drv_t *)s_disp_drv);
        s_disp_drv = NULL;
    }
}
#else
/**
 * @file lv_port_disp.c
 * @brief LVGL 显示接口层 (极速架构：内部 SRAM 局部渲染 + DMA2D 硬件搬运 + 防死锁保护)
 */

#include "lv_port_disp_template.h"
#include "lvgl.h"
#include "stm32f4xx_hal.h"
#include "bsp_sdram.h"

/* 确保在这里定义了你的 SDRAM 显存起始地址 */
#ifndef LCD_FRAME_BUF_ADDR1
#define LCD_FRAME_BUF_ADDR1 0xD0000000
#endif

#define MY_DISP_HOR_RES    800
#define MY_DISP_VER_RES    480

/**********************
 * STATIC PROTOTYPES
 **********************/
static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);

/**********************
 * STATIC VARIABLES
 **********************/
/* 核心优化：在单片机超高速的内部 SRAM 中开辟一块局部画板（40 行约 64KB）。
 * CPU 在这上面计算抗锯齿和透明度的速度，是在外部 SDRAM 上算的几十倍！
 */
static lv_color_t buf_1[MY_DISP_HOR_RES * 40];

extern LTDC_HandleTypeDef hltdc;
extern SDRAM_HandleTypeDef hsdram2;

/**********************
 * GLOBAL FUNCTIONS
 **********************/
void lv_port_disp_init(void)
{
    /* 1. 硬件初始化 */
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_7, GPIO_PIN_SET);
    
    /* 确保 LTDC 永远只盯着这一块固定的 SDRAM 显存看，不要翻转 */
    HAL_LTDC_SetAddress(&hltdc, LCD_FRAME_BUF_ADDR1, 0);

    /* 2. 初始化局部绘制缓冲区 */
    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf_1, NULL, MY_DISP_HOR_RES * 40);

    /* 3. 注册显示驱动 */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    
    disp_drv.hor_res      = MY_DISP_HOR_RES;
    disp_drv.ver_res      = MY_DISP_VER_RES;
    disp_drv.flush_cb     = disp_flush;
    disp_drv.draw_buf     = &draw_buf;
    
    /* 开启局部刷新(full_refresh=0)，释放 CPU  */
    disp_drv.full_refresh = 0; 

    lv_disp_drv_register(&disp_drv);
}

/**********************
 * FLUSH CALLBACK
 **********************/
/**
 * @brief  LVGL flush 回调 —— 使用 DMA2D 将内部 SRAM 的画面极速盖到 SDRAM 上
 */
static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t width = area->x2 - area->x1 + 1;
    uint32_t height = area->y2 - area->y1 + 1;
    
    /* 计算本次刷新的起始像素点在整个 800x480 屏幕中的偏移量 */
    uint32_t offset = area->y1 * MY_DISP_HOR_RES + area->x1;
    
    /* 计算在外部 SDRAM 中的绝对物理地址 (假设使用 RGB565，每个像素占 2 字节) */
    uint32_t dest_addr = LCD_FRAME_BUF_ADDR1 + (offset * 2);

    /* ======== 寄存器级 DMA2D 操作 (绕过复杂的 HAL 库句柄，纯硬件暴力搬运) ======== */
    
    /* 配置为内存到内存 (M2M) 模式 */
    DMA2D->CR      = 0x00000000UL; 
    
    /* 配置源数据 (LVGL 在内部 SRAM 画好的局布图) */
    DMA2D->FGPFCCR = 0x00000002UL; /* 输入格式：RGB565 */
    DMA2D->FGOR    = 0;            /* 源数据没有行间隙 */
    DMA2D->FGMAR   = (uint32_t)color_p;
    
    /* 配置目标数据 (外部 SDRAM 显示区域) */
    DMA2D->OPFCCR  = 0x00000002UL; /* 输出格式：RGB565 */
    DMA2D->OOR     = MY_DISP_HOR_RES - width; /* 跨行时的偏移量补偿 */
    DMA2D->OMAR    = dest_addr;
    
    /* 配置搬运尺寸 (宽和高) */
    DMA2D->NLR     = (uint32_t)(width << 16) | (uint16_t)height;
    
    /* 发发发！启动 DMA2D 传输 */
    DMA2D->CR     |= DMA2D_CR_START;
    
    /* ======== 核心防死锁机制 ======== */
    /* 设定超时阈值。根据主频 180MHz 计算，0xFFFFFF 足够容纳一次正常的 DMA2D 局部搬运 */
    uint32_t timeout = 0x00FFFFFF; 
    
    /* 增加超时判断，防止 START 位无法清零导致永久死循环 */
    while ((DMA2D->CR & DMA2D_CR_START) && (timeout > 0)) {
        timeout--;
    }
    
    /* 异常处理：判断是否因为硬件错误或超时而退出 */
    if ((DMA2D->ISR & DMA2D_ISR_TEIF) || (timeout == 0)) {
        /* 1. 如果是传输错误，写入 CTEIF 清除错误标志位，解除硬件锁定 */
        DMA2D->IFCR = DMA2D_IFCR_CTEIF; 
        
        /* 2. 强制复位 START 控制位，丢弃当前这块损坏的局部渲染，保护主循环不被拖死 */
        DMA2D->CR &= ~DMA2D_CR_START;
    }

    /* 通知 LVGL 这一小块刷新完了（无论成功还是超时丢弃，都必须通知，否则 LVGL 内部状态机会卡死） */
    lv_disp_flush_ready(disp_drv);
}
#endif
