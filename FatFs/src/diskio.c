/*-----------------------------------------------------------------------*/
/* Low level disk I/O module for FatFs - STM32F429 SDIO + DMA            */
/*-----------------------------------------------------------------------*/

#include "diskio.h"
#include "sdio.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern SD_HandleTypeDef hsd;

#define SD_BLOCK_SIZE_BYTES         512U
#define DISKIO_SD_INIT_TIMEOUT_MS   2000U
#define DISKIO_SD_READ_TIMEOUT_MS   5000U
#define DISKIO_SD_WRITE_TIMEOUT_MS  15000U
#define DISKIO_SD_SYNC_TIMEOUT_MS   15000U
#define DISKIO_MAX_IO_TRACE         12U

/* STM32F429 CCM RAM is not accessible by DMA. */
#define STM32F429_CCMRAM_START      0x10000000UL
#define STM32F429_CCMRAM_END        0x10010000UL

#ifndef HAL_SD_ERROR_NONE
#define HAL_SD_ERROR_NONE           0x00000000U
#endif

static volatile DSTATUS sd_stat = STA_NOINIT;
static volatile uint8_t sd_rx_done = 0U;
static volatile uint8_t sd_tx_done = 0U;
static volatile uint8_t sd_xfer_error = 0U;
static volatile uint32_t sd_last_error = HAL_SD_ERROR_NONE;
static uint32_t sd_read_trace_count = 0U;
static uint32_t sd_write_trace_count = 0U;

#if defined ( __ICCARM__ )
#pragma data_alignment=4
#endif
static uint8_t sd_dma_bounce[SD_BLOCK_SIZE_BYTES]
#if defined ( __CC_ARM ) || defined ( __GNUC__ )
__attribute__((aligned(4)))
#endif
;

static uint8_t SD_TimeoutExpired(uint32_t start_tick, uint32_t timeout_ms)
{
    return ((uint32_t)(HAL_GetTick() - start_tick) >= timeout_ms) ? 1U : 0U;
}

static uint8_t SD_IsDMABufferOK(const void *ptr, uint32_t len)
{
    uintptr_t start = (uintptr_t)ptr;
    uintptr_t end;

    if ((ptr == NULL) || (len == 0U))
    {
        return 0U;
    }

    end = start + (uintptr_t)len - 1U;

    if ((start & 0x03U) != 0U)
    {
        return 0U;
    }

    if ((start < (uintptr_t)STM32F429_CCMRAM_END) &&
        (end >= (uintptr_t)STM32F429_CCMRAM_START))
    {
        return 0U;
    }

    return 1U;
}

static void SD_ClearTransferFlags(void)
{
    sd_rx_done = 0U;
    sd_tx_done = 0U;
    sd_xfer_error = 0U;
    sd_last_error = HAL_SD_ERROR_NONE;
    hsd.ErrorCode = HAL_SD_ERROR_NONE;
}

static DRESULT SD_WaitCardTransferState(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while (HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER)
    {
        if (SD_TimeoutExpired(start, timeout_ms) != 0U)
        {
            sd_last_error = hsd.ErrorCode;
            printf("[diskio][ERROR] Card transfer-state timeout. HAL state=0x%08lX error=0x%08lX\r\n",
                   (unsigned long)hsd.State, (unsigned long)hsd.ErrorCode);
            return RES_NOTRDY;
        }
    }

    return RES_OK;
}

static DRESULT SD_WaitDmaTransferDone(uint8_t is_write, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    for (;;)
    {
        if ((sd_xfer_error != 0U) || (hsd.ErrorCode != HAL_SD_ERROR_NONE))
        {
            sd_last_error = hsd.ErrorCode;
            printf("[diskio][ERROR] DMA transfer error. is_write=%u HAL state=0x%08lX error=0x%08lX\r\n",
                   is_write, (unsigned long)hsd.State, (unsigned long)hsd.ErrorCode);
            return RES_ERROR;
        }

        if (is_write != 0U)
        {
            if (sd_tx_done != 0U)
            {
                break;
            }
        }
        else
        {
            if (sd_rx_done != 0U)
            {
                break;
            }
        }

        if (SD_TimeoutExpired(start, timeout_ms) != 0U)
        {
            sd_last_error = hsd.ErrorCode;
            printf("[diskio][ERROR] DMA transfer timeout. is_write=%u HAL state=0x%08lX error=0x%08lX rx=%u tx=%u xerr=%u\r\n",
                   is_write, (unsigned long)hsd.State, (unsigned long)hsd.ErrorCode,
                   sd_rx_done, sd_tx_done, sd_xfer_error);
            return RES_NOTRDY;
        }
    }

    return SD_WaitCardTransferState(timeout_ms);
}

void HAL_SD_RxCpltCallback(SD_HandleTypeDef *hsd_cb)
{
    if (hsd_cb == &hsd)
    {
        sd_rx_done = 1U;
    }
}

void HAL_SD_TxCpltCallback(SD_HandleTypeDef *hsd_cb)
{
    if (hsd_cb == &hsd)
    {
        sd_tx_done = 1U;
    }
}

void HAL_SD_ErrorCallback(SD_HandleTypeDef *hsd_cb)
{
    if (hsd_cb == &hsd)
    {
        sd_xfer_error = 1U;
        sd_last_error = hsd_cb->ErrorCode;
    }
}

DSTATUS disk_initialize(BYTE pdrv)
{
    HAL_StatusTypeDef hal_status;
    HAL_SD_CardStateTypeDef card_state;

    if (pdrv != DEV_SD)
    {
        return STA_NOINIT;
    }

    sd_stat = STA_NOINIT;
    SD_ClearTransferFlags();

    printf("[diskio][INIT] start. HAL state=0x%08lX error=0x%08lX\r\n",
           (unsigned long)hsd.State, (unsigned long)hsd.ErrorCode);

    if (hsd.State == HAL_SD_STATE_RESET)
    {
        printf("[diskio][INIT] HAL handle is RESET, running HAL_SD_Init.\r\n");

        hal_status = HAL_SD_Init(&hsd);
        if (hal_status != HAL_OK)
        {
            sd_last_error = hsd.ErrorCode;
            printf("[diskio][ERROR] HAL_SD_Init failed. status=%d error=0x%08lX\r\n",
                   hal_status, (unsigned long)hsd.ErrorCode);
            return sd_stat;
        }

        hal_status = HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B);
        if (hal_status != HAL_OK)
        {
            sd_last_error = hsd.ErrorCode;
            printf("[diskio][ERROR] HAL_SD_ConfigWideBusOperation failed. status=%d error=0x%08lX\r\n",
                   hal_status, (unsigned long)hsd.ErrorCode);
            return sd_stat;
        }
    }
    else if (hsd.State != HAL_SD_STATE_READY)
    {
        printf("[diskio][INIT] HAL state is busy, aborting stale transfer. state=0x%08lX\r\n",
               (unsigned long)hsd.State);
        (void)HAL_SD_Abort(&hsd);
    }
    else
    {
        printf("[diskio][INIT] Reusing SDIO initialized by MX_SDIO_SD_Init.\r\n");
    }

    card_state = HAL_SD_GetCardState(&hsd);
    printf("[diskio][INIT] card_state=0x%08lX HAL state=0x%08lX error=0x%08lX\r\n",
           (unsigned long)card_state, (unsigned long)hsd.State, (unsigned long)hsd.ErrorCode);

    if (SD_WaitCardTransferState(DISKIO_SD_INIT_TIMEOUT_MS) == RES_OK)
    {
        sd_stat &= (DSTATUS)~STA_NOINIT;
        printf("[diskio][INIT] ready.\r\n");
    }
    else
    {
        printf("[diskio][INIT] not ready.\r\n");
    }

    return sd_stat;
}

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != DEV_SD)
    {
        return STA_NOINIT;
    }

    return sd_stat;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    DRESULT res;
    uint32_t byte_count;
    uint32_t sector32;
    UINT i;

    if ((pdrv != DEV_SD) || (buff == NULL) || (count == 0U))
    {
        return RES_PARERR;
    }

    if ((sd_stat & STA_NOINIT) != 0U)
    {
        return RES_NOTRDY;
    }

    byte_count = (uint32_t)count * SD_BLOCK_SIZE_BYTES;
    sector32 = (uint32_t)sector;

    if (sd_read_trace_count < DISKIO_MAX_IO_TRACE)
    {
        printf("[diskio][READ] sector=%lu count=%u buff=0x%08lX direct=%u\r\n",
               (unsigned long)sector32, count, (unsigned long)(uintptr_t)buff,
               SD_IsDMABufferOK(buff, byte_count));
        sd_read_trace_count++;
    }

    if (SD_IsDMABufferOK(buff, byte_count) != 0U)
    {
        res = SD_WaitCardTransferState(DISKIO_SD_READ_TIMEOUT_MS);
        if (res != RES_OK)
        {
            return res;
        }

        SD_ClearTransferFlags();
        if (HAL_SD_ReadBlocks_DMA(&hsd, buff, sector32, count) != HAL_OK)
        {
            sd_last_error = hsd.ErrorCode;
            printf("[diskio][ERROR] HAL_SD_ReadBlocks_DMA failed. sector=%lu count=%u state=0x%08lX error=0x%08lX\r\n",
                   (unsigned long)sector32, count,
                   (unsigned long)hsd.State, (unsigned long)hsd.ErrorCode);
            return RES_ERROR;
        }

        return SD_WaitDmaTransferDone(0U, DISKIO_SD_READ_TIMEOUT_MS);
    }

    for (i = 0U; i < count; i++)
    {
        res = SD_WaitCardTransferState(DISKIO_SD_READ_TIMEOUT_MS);
        if (res != RES_OK)
        {
            return res;
        }

        SD_ClearTransferFlags();
        if (HAL_SD_ReadBlocks_DMA(&hsd, sd_dma_bounce, sector32 + i, 1U) != HAL_OK)
        {
            sd_last_error = hsd.ErrorCode;
            printf("[diskio][ERROR] HAL_SD_ReadBlocks_DMA bounce failed. sector=%lu state=0x%08lX error=0x%08lX\r\n",
                   (unsigned long)(sector32 + i),
                   (unsigned long)hsd.State, (unsigned long)hsd.ErrorCode);
            return RES_ERROR;
        }

        res = SD_WaitDmaTransferDone(0U, DISKIO_SD_READ_TIMEOUT_MS);
        if (res != RES_OK)
        {
            return res;
        }

        memcpy(&buff[i * SD_BLOCK_SIZE_BYTES], sd_dma_bounce, SD_BLOCK_SIZE_BYTES);
    }

    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    DRESULT res;
    uint32_t byte_count;
    uint32_t sector32;
    UINT i;

    if ((pdrv != DEV_SD) || (buff == NULL) || (count == 0U))
    {
        return RES_PARERR;
    }

    if ((sd_stat & STA_NOINIT) != 0U)
    {
        return RES_NOTRDY;
    }

    byte_count = (uint32_t)count * SD_BLOCK_SIZE_BYTES;
    sector32 = (uint32_t)sector;

    if (sd_write_trace_count < DISKIO_MAX_IO_TRACE)
    {
        printf("[diskio][WRITE] sector=%lu count=%u buff=0x%08lX direct=%u\r\n",
               (unsigned long)sector32, count, (unsigned long)(uintptr_t)buff,
               SD_IsDMABufferOK(buff, byte_count));
        sd_write_trace_count++;
    }

    if (SD_IsDMABufferOK(buff, byte_count) != 0U)
    {
        res = SD_WaitCardTransferState(DISKIO_SD_WRITE_TIMEOUT_MS);
        if (res != RES_OK)
        {
            return res;
        }

        SD_ClearTransferFlags();
        if (HAL_SD_WriteBlocks_DMA(&hsd, (uint8_t *)buff, sector32, count) != HAL_OK)
        {
            sd_last_error = hsd.ErrorCode;
            printf("[diskio][ERROR] HAL_SD_WriteBlocks_DMA failed. sector=%lu count=%u state=0x%08lX error=0x%08lX\r\n",
                   (unsigned long)sector32, count,
                   (unsigned long)hsd.State, (unsigned long)hsd.ErrorCode);
            return RES_ERROR;
        }

        return SD_WaitDmaTransferDone(1U, DISKIO_SD_WRITE_TIMEOUT_MS);
    }

    for (i = 0U; i < count; i++)
    {
        memcpy(sd_dma_bounce, &buff[i * SD_BLOCK_SIZE_BYTES], SD_BLOCK_SIZE_BYTES);

        res = SD_WaitCardTransferState(DISKIO_SD_WRITE_TIMEOUT_MS);
        if (res != RES_OK)
        {
            return res;
        }

        SD_ClearTransferFlags();
        if (HAL_SD_WriteBlocks_DMA(&hsd, sd_dma_bounce, sector32 + i, 1U) != HAL_OK)
        {
            sd_last_error = hsd.ErrorCode;
            printf("[diskio][ERROR] HAL_SD_WriteBlocks_DMA bounce failed. sector=%lu state=0x%08lX error=0x%08lX\r\n",
                   (unsigned long)(sector32 + i),
                   (unsigned long)hsd.State, (unsigned long)hsd.ErrorCode);
            return RES_ERROR;
        }

        res = SD_WaitDmaTransferDone(1U, DISKIO_SD_WRITE_TIMEOUT_MS);
        if (res != RES_OK)
        {
            return res;
        }
    }

    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    DRESULT res = RES_ERROR;
    HAL_SD_CardInfoTypeDef card_info;

    if (pdrv != DEV_SD)
    {
        return RES_PARERR;
    }

    if ((sd_stat & STA_NOINIT) != 0U)
    {
        return RES_NOTRDY;
    }

    switch (cmd)
    {
        case CTRL_SYNC:
            res = SD_WaitCardTransferState(DISKIO_SD_SYNC_TIMEOUT_MS);
            break;

        case GET_SECTOR_COUNT:
            if ((buff != NULL) && (HAL_SD_GetCardInfo(&hsd, &card_info) == HAL_OK))
            {
                *(LBA_t *)buff = (LBA_t)card_info.LogBlockNbr;
                res = RES_OK;
            }
            break;

        case GET_SECTOR_SIZE:
            if ((buff != NULL) && (HAL_SD_GetCardInfo(&hsd, &card_info) == HAL_OK))
            {
                *(WORD *)buff = (WORD)(card_info.LogBlockSize != 0U ?
                                       card_info.LogBlockSize : SD_BLOCK_SIZE_BYTES);
                res = RES_OK;
            }
            break;

        case GET_BLOCK_SIZE:
            if (buff != NULL)
            {
                *(DWORD *)buff = 1U;
                res = RES_OK;
            }
            break;

        case CTRL_TRIM:
            res = RES_OK;
            break;

        default:
            res = RES_PARERR;
            break;
    }

    if (res != RES_OK)
    {
        printf("[diskio][ERROR] ioctl failed. cmd=%u res=%d error=0x%08lX\r\n",
               cmd, res, (unsigned long)hsd.ErrorCode);
    }

    return res;
}

DWORD get_fattime(void)
{
    return  ((DWORD)(2026U - 1980U) << 25)
          | ((DWORD)5U << 21)
          | ((DWORD)15U << 16)
          | ((DWORD)12U << 11)
          | ((DWORD)0U << 5)
          | ((DWORD)0U);
}

uint32_t diskio_sd_last_error(void)
{
    return sd_last_error;
}
