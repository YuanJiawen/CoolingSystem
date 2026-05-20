/**
  ******************************************************************************
  * @file    sd_test.h
  * @brief   Optional raw SDIO + DMA stress test interface.
  ******************************************************************************
  */
#ifndef __SD_TEST_H
#define __SD_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/*
 * Raw sector testing is destructive. Keep this disabled for normal FatFs tests.
 * Define SD_STRESS_TEST_ENABLE_RAW_WRITE=1 in the compiler symbols, or change
 * the value below, only when the SD card can be overwritten.
 */
#ifndef SD_STRESS_TEST_ENABLE_RAW_WRITE
#define SD_STRESS_TEST_ENABLE_RAW_WRITE 0
#endif

void SD_StressTest(void);

#ifdef __cplusplus
}
#endif

#endif /* __SD_TEST_H */
