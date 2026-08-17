/**
 * @file    fake/sdio.h
 * @brief   SDIO 回归测试用 sdio.h 桩
 * @note    SDIO_VOID_INIT:旧版签名(修复前,void 返回),用于红验证;
 *          未定义时为修复后签名(uint8_t 返回),用于绿验证。
 */
#ifndef FAKE_SDIO_H
#define FAKE_SDIO_H

#include <stdint.h>
#include "main.h"

extern SD_HandleTypeDef hsd;

#ifdef SDIO_VOID_INIT
void MX_SDIO_SD_Init(void);
#else
uint8_t MX_SDIO_SD_Init(void);
#endif

#endif /* FAKE_SDIO_H */
