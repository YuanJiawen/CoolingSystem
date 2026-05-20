/*
 * bsp_sdram.h
 * * 适配硬件: Winbond W9825G6KH-6 (32MB)
 * 连接方式: FMC Bank 2 (PH6/PH7)
 */

#ifndef __BSP_SDRAM_H
#define __BSP_SDRAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ====================================================================
 * SDRAM 地址映射定义
 * --------------------------------------------------------------------
 * STM32F429 FMC SDRAM Bank 地址范围:
 * Bank 1: 0xC0000000 - 0xCFFFFFFF
 * Bank 2: 0xD0000000 - 0xDFFFFFFF (当前使用)
 * ==================================================================== */
#define SDRAM_BANK_ADDR     ((uint32_t)0xD0000000)

/* SDRAM 大小 (W9825G6 = 32MB = 0x2000000) */
#define SDRAM_SIZE          ((uint32_t)0x2000000)

/* 超时时间定义 */
#define SDRAM_TIMEOUT       ((uint32_t)0xFFFF)

/* ====================================================================
 * 函数声明
 * ==================================================================== */

/**
 * @brief  执行 SDRAM 初始化序列 (上电后必须执行)
 * @note   在 MX_FMC_Init() 之后调用
 * @param  hsdram: 指向 SDRAM 句柄的指针
 */
void BSP_SDRAM_Init_Sequence(SDRAM_HandleTypeDef *hsdram);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_SDRAM_H */
