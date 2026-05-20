/**
  ******************************************************************************
  * @file    sd_test.c
  * @brief   Optional raw SDIO + DMA stress test.
  ******************************************************************************
  */

#include "sd_test.h"
#include "sdio.h"
#include <stdio.h>
#include <string.h>

#define SD_TEST_BLOCK_SIZE_BYTES     512U
#define SD_TEST_BLOCK_COUNT          8U
#define SD_TEST_START_SECTOR         10000UL
#define SD_TEST_CYCLES               200UL
#define SD_TEST_TIMEOUT_MS           15000UL

extern SD_HandleTypeDef hsd;

#if (SD_STRESS_TEST_ENABLE_RAW_WRITE != 0)

#if defined ( __ICCARM__ )
#pragma data_alignment=4
#elif defined ( __CC_ARM ) || defined ( __GNUC__ )
__attribute__((aligned(4)))
#endif
static uint8_t SD_TxBuffer[SD_TEST_BLOCK_SIZE_BYTES * SD_TEST_BLOCK_COUNT];

#if defined ( __ICCARM__ )
#pragma data_alignment=4
#elif defined ( __CC_ARM ) || defined ( __GNUC__ )
__attribute__((aligned(4)))
#endif
static uint8_t SD_RxBuffer[SD_TEST_BLOCK_SIZE_BYTES * SD_TEST_BLOCK_COUNT];

static uint8_t SD_TimeoutExpired(uint32_t start_tick, uint32_t timeout_ms)
{
    return ((uint32_t)(HAL_GetTick() - start_tick) >= timeout_ms) ? 1U : 0U;
}

static uint8_t SD_WaitReady(uint32_t timeout_ms)
{
    uint32_t start;

    start = HAL_GetTick();
    while (hsd.State != HAL_SD_STATE_READY)
    {
        if (SD_TimeoutExpired(start, timeout_ms) != 0U)
        {
            printf("[RAW-SD][ERROR] HAL state timeout. State=0x%08lX Error=0x%08lX\r\n",
                   (unsigned long)hsd.State, (unsigned long)hsd.ErrorCode);
            return 1U;
        }
    }

    start = HAL_GetTick();
    while (HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER)
    {
        if (SD_TimeoutExpired(start, timeout_ms) != 0U)
        {
            printf("[RAW-SD][ERROR] Card transfer-state timeout. State=0x%08lX Error=0x%08lX\r\n",
                   (unsigned long)hsd.State, (unsigned long)hsd.ErrorCode);
            return 1U;
        }
    }

    return 0U;
}

static void SD_FillPattern(uint32_t cycle)
{
    uint32_t i;

    for (i = 0UL; i < sizeof(SD_TxBuffer); i++)
    {
        SD_TxBuffer[i] = (uint8_t)((i ^ (i >> 3) ^ cycle ^ 0x5AU) & 0xFFU);
    }
}

static void SD_PrintFirstMismatch(void)
{
    uint32_t i;
    uint32_t printed;

    printed = 0UL;
    for (i = 0UL; i < sizeof(SD_TxBuffer); i++)
    {
        if (SD_TxBuffer[i] != SD_RxBuffer[i])
        {
            printf("[RAW-SD][ERROR] Mismatch byte %lu: expected=0x%02X actual=0x%02X\r\n",
                   (unsigned long)i, SD_TxBuffer[i], SD_RxBuffer[i]);
            printed++;
            if (printed >= 16UL)
            {
                break;
            }
        }
    }
}

#endif /* SD_STRESS_TEST_ENABLE_RAW_WRITE */

void SD_StressTest(void)
{
#if (SD_STRESS_TEST_ENABLE_RAW_WRITE == 0)
    printf("\r\n==========================================\r\n");
    printf("=== Raw SDIO DMA Stress Test SKIPPED ===\r\n");
    printf("==========================================\r\n");
    printf("[RAW-SD][INFO] Raw sector writes are disabled by default.\r\n");
    printf("[RAW-SD][INFO] Set SD_STRESS_TEST_ENABLE_RAW_WRITE to 1 only with a disposable SD card.\r\n");
#else
    HAL_SD_CardInfoTypeDef card_info;
    HAL_StatusTypeDef hal_status;
    uint32_t cycle;
    uint32_t error_count;
    uint32_t total_sectors;
    uint32_t test_end_sector;

    error_count = 0UL;
    total_sectors = 0UL;
    test_end_sector = SD_TEST_START_SECTOR + SD_TEST_BLOCK_COUNT;

    printf("\r\n==========================================\r\n");
    printf("=== Raw SDIO DMA Stress Test Starting ===\r\n");
    printf("==========================================\r\n");
    printf("[RAW-SD][WARN] This test overwrites raw sectors and can corrupt a file system.\r\n");
    printf("[RAW-SD][INFO] Start sector: %lu\r\n", SD_TEST_START_SECTOR);
    printf("[RAW-SD][INFO] Blocks/cycle: %lu\r\n", (unsigned long)SD_TEST_BLOCK_COUNT);
    printf("[RAW-SD][INFO] Bytes/cycle : %lu\r\n",
           (unsigned long)(SD_TEST_BLOCK_SIZE_BYTES * SD_TEST_BLOCK_COUNT));
    printf("[RAW-SD][INFO] Cycles      : %lu\r\n", SD_TEST_CYCLES);

    if (HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER)
    {
        printf("[RAW-SD][ERROR] SD card is not ready. HAL state=0x%08lX Error=0x%08lX\r\n",
               (unsigned long)hsd.State, (unsigned long)hsd.ErrorCode);
        return;
    }

    hal_status = HAL_SD_GetCardInfo(&hsd, &card_info);
    if (hal_status != HAL_OK)
    {
        printf("[RAW-SD][ERROR] HAL_SD_GetCardInfo failed. status=%d Error=0x%08lX\r\n",
               hal_status, (unsigned long)hsd.ErrorCode);
        return;
    }

    total_sectors = card_info.LogBlockNbr;
    printf("[RAW-SD][INFO] Card sectors: %lu, block size: %lu\r\n",
           (unsigned long)total_sectors, (unsigned long)card_info.LogBlockSize);

    if ((total_sectors == 0UL) || (test_end_sector > total_sectors))
    {
        printf("[RAW-SD][ERROR] Test range exceeds card capacity. end=%lu total=%lu\r\n",
               (unsigned long)test_end_sector, (unsigned long)total_sectors);
        return;
    }

    for (cycle = 1UL; cycle <= SD_TEST_CYCLES; cycle++)
    {
        SD_FillPattern(cycle);
        memset(SD_RxBuffer, 0, sizeof(SD_RxBuffer));

        hsd.ErrorCode = HAL_SD_ERROR_NONE;
        hal_status = HAL_SD_WriteBlocks_DMA(&hsd, SD_TxBuffer,
                                            SD_TEST_START_SECTOR,
                                            SD_TEST_BLOCK_COUNT);
        if (hal_status != HAL_OK)
        {
            printf("[RAW-SD][ERROR] Cycle %lu write command failed. status=%d Error=0x%08lX\r\n",
                   (unsigned long)cycle, hal_status, (unsigned long)hsd.ErrorCode);
            error_count++;
            continue;
        }

        if (SD_WaitReady(SD_TEST_TIMEOUT_MS) != 0U)
        {
            printf("[RAW-SD][ERROR] Cycle %lu write wait failed.\r\n", (unsigned long)cycle);
            error_count++;
            continue;
        }

        hsd.ErrorCode = HAL_SD_ERROR_NONE;
        hal_status = HAL_SD_ReadBlocks_DMA(&hsd, SD_RxBuffer,
                                           SD_TEST_START_SECTOR,
                                           SD_TEST_BLOCK_COUNT);
        if (hal_status != HAL_OK)
        {
            printf("[RAW-SD][ERROR] Cycle %lu read command failed. status=%d Error=0x%08lX\r\n",
                   (unsigned long)cycle, hal_status, (unsigned long)hsd.ErrorCode);
            error_count++;
            continue;
        }

        if (SD_WaitReady(SD_TEST_TIMEOUT_MS) != 0U)
        {
            printf("[RAW-SD][ERROR] Cycle %lu read wait failed.\r\n", (unsigned long)cycle);
            error_count++;
            continue;
        }

        if (memcmp(SD_TxBuffer, SD_RxBuffer, sizeof(SD_TxBuffer)) != 0)
        {
            printf("[RAW-SD][ERROR] Cycle %lu data verification failed.\r\n", (unsigned long)cycle);
            SD_PrintFirstMismatch();
            error_count++;
            continue;
        }

        if ((cycle % 10UL) == 0UL)
        {
            printf("[RAW-SD][INFO] Cycle %lu / %lu OK\r\n",
                   (unsigned long)cycle, SD_TEST_CYCLES);
        }
    }

    printf("\r\n==========================================\r\n");
    if (error_count == 0UL)
    {
        printf("=== Raw SDIO DMA Stress Test PASS ===\r\n");
    }
    else
    {
        printf("=== Raw SDIO DMA Stress Test FAIL ===\r\n");
        printf("[RAW-SD][INFO] Error count: %lu\r\n", (unsigned long)error_count);
    }
    printf("==========================================\r\n");
#endif
}
