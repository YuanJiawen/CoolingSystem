/**
  ******************************************************************************
  * @file    fatfs_test.h
  * @brief   FatFs 文件系统综合测试头文件
  ******************************************************************************
  */
#ifndef __FATFS_TEST_H
#define __FATFS_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ff.h"    /* 引入 FatFs 核心头文件 */
#include "main.h"  /* 引入 HAL 库及基础定义 */

/* 导出函数声明 */
void FatFs_IntegrationTest(void);

#ifdef __cplusplus
}
#endif

#endif /* __FATFS_TEST_H */