/**
  ******************************************************************************
  * @file    fatfs_test.c
  * @brief   FatFs integration and endurance-oriented validation tests.
  ******************************************************************************
  */

#include "fatfs_test.h"
#include <stdio.h>
#include <string.h>

#define FATFS_TEST_DRIVE              "0:"
#define FATFS_TEST_DIR                "0:/TestDir"
#define FATFS_TEST_TEXT_FILE          "0:/TestDir/test.txt"
#define FATFS_TEST_PATTERN_FILE       "0:/TestDir/pattern.bin"
#define FATFS_TEST_PATTERN_BYTES      (64UL * 1024UL)
#define FATFS_TEST_CHUNK_BYTES        512U
#define FATFS_TEST_PROGRESS_BYTES     (16UL * 1024UL)

static FATFS fs;
static FIL file;

/* The +1 byte lets the test deliberately pass an unaligned buffer to FatFs. */
static uint8_t write_buff[FATFS_TEST_CHUNK_BYTES + 1U];
static uint8_t read_buff[FATFS_TEST_CHUNK_BYTES + 1U];

static const char *test_string =
    "Hello STM32F429! FatFs small-file read/write verification.\r\n";

static void FatFs_FillPattern(uint8_t *buff, UINT len, DWORD offset)
{
    UINT i;

    for (i = 0U; i < len; i++)
    {
        DWORD pos = offset + (DWORD)i;
        buff[i] = (uint8_t)((pos ^ (pos >> 3) ^ (pos >> 11) ^ 0xA5U) & 0xFFU);
    }
}

static uint8_t FatFs_CheckResult(FRESULT fres, const char *operation)
{
    if (fres == FR_OK)
    {
        return 0U;
    }

    printf("[FatFs][ERROR] %s failed. FRESULT=%d\r\n", operation, fres);
    return 1U;
}

static FRESULT FatFs_UnlinkIfExists(const char *path)
{
    FRESULT fres;

    fres = f_unlink(path);
    if ((fres == FR_OK) || (fres == FR_NO_FILE) || (fres == FR_NO_PATH))
    {
        return FR_OK;
    }

    printf("[FatFs][ERROR] Cannot remove old file %s. FRESULT=%d\r\n", path, fres);
    return fres;
}

static uint8_t FatFs_PrintCapacity(void)
{
    FRESULT fres;
    DWORD fre_clust;
    DWORD fre_sect;
    DWORD tot_sect;
    FATFS *pfs;

    printf("[FatFs][INFO] Reading volume capacity...\r\n");

    pfs = &fs;
    fres = f_getfree(FATFS_TEST_DRIVE, &fre_clust, &pfs);
    if (FatFs_CheckResult(fres, "f_getfree") != 0U)
    {
        return 1U;
    }

    tot_sect = (pfs->n_fatent - 2U) * pfs->csize;
    fre_sect = fre_clust * pfs->csize;

    printf("[FatFs][INFO] Total: %lu KB (%lu MB)\r\n",
           tot_sect / 2U, tot_sect / 2048U);
    printf("[FatFs][INFO] Free : %lu KB (%lu MB)\r\n",
           fre_sect / 2U, fre_sect / 2048U);

    return 0U;
}

static uint8_t FatFs_PrepareTestDirectory(void)
{
    FRESULT fres;

    printf("[FatFs][STEP] Preparing test directory %s\r\n", FATFS_TEST_DIR);

    fres = f_mkdir(FATFS_TEST_DIR);
    if ((fres != FR_OK) && (fres != FR_EXIST))
    {
        printf("[FatFs][ERROR] f_mkdir failed. FRESULT=%d\r\n", fres);
        return 1U;
    }

    if (FatFs_UnlinkIfExists(FATFS_TEST_TEXT_FILE) != FR_OK)
    {
        return 1U;
    }

    if (FatFs_UnlinkIfExists(FATFS_TEST_PATTERN_FILE) != FR_OK)
    {
        return 1U;
    }

    printf("[FatFs][PASS] Test directory is ready.\r\n");
    return 0U;
}

static uint8_t FatFs_RunSmallFileTest(void)
{
    FRESULT fres;
    UINT bytes_written;
    UINT bytes_read;
    UINT text_len;
    uint8_t failed;

    failed = 0U;
    bytes_written = 0U;
    bytes_read = 0U;
    text_len = (UINT)strlen(test_string);

    printf("[FatFs][STEP] Small file write/read test.\r\n");

    memset(write_buff, 0, sizeof(write_buff));
    memset(read_buff, 0, sizeof(read_buff));
    memcpy(write_buff, test_string, text_len);

    fres = f_open(&file, FATFS_TEST_TEXT_FILE, FA_CREATE_ALWAYS | FA_WRITE);
    if (FatFs_CheckResult(fres, "f_open small file for write") != 0U)
    {
        return 1U;
    }

    fres = f_write(&file, write_buff, text_len, &bytes_written);
    if ((fres != FR_OK) || (bytes_written != text_len))
    {
        printf("[FatFs][ERROR] f_write small file failed. FRESULT=%d, bytes=%u/%u\r\n",
               fres, bytes_written, text_len);
        failed = 1U;
    }

    if (failed == 0U)
    {
        fres = f_sync(&file);
        if (FatFs_CheckResult(fres, "f_sync small file") != 0U)
        {
            failed = 1U;
        }
    }

    fres = f_close(&file);
    if (FatFs_CheckResult(fres, "f_close small file after write") != 0U)
    {
        failed = 1U;
    }

    if (failed != 0U)
    {
        return 1U;
    }

    fres = f_open(&file, FATFS_TEST_TEXT_FILE, FA_READ);
    if (FatFs_CheckResult(fres, "f_open small file for read") != 0U)
    {
        return 1U;
    }

    fres = f_read(&file, read_buff, text_len, &bytes_read);
    if ((fres != FR_OK) || (bytes_read != text_len))
    {
        printf("[FatFs][ERROR] f_read small file failed. FRESULT=%d, bytes=%u/%u\r\n",
               fres, bytes_read, text_len);
        failed = 1U;
    }
    else if (memcmp(write_buff, read_buff, text_len) != 0)
    {
        printf("[FatFs][ERROR] Small file data mismatch.\r\n");
        failed = 1U;
    }

    fres = f_close(&file);
    if (FatFs_CheckResult(fres, "f_close small file after read") != 0U)
    {
        failed = 1U;
    }

    if (failed == 0U)
    {
        printf("[FatFs][PASS] Small file verified. Bytes=%u\r\n", text_len);
    }

    return failed;
}

static uint8_t FatFs_RunPatternFileWrite(void)
{
    FRESULT fres;
    UINT bytes_written;
    UINT chunk;
    DWORD offset;
    DWORD remaining;
    DWORD next_progress;
    uint8_t *unaligned_write;
    uint8_t failed;

    printf("[FatFs][STEP] Pattern file write test. Size=%lu bytes\r\n",
           FATFS_TEST_PATTERN_BYTES);
    printf("[FatFs][INFO] Using an intentionally unaligned user buffer.\r\n");

    failed = 0U;
    offset = 0UL;
    next_progress = FATFS_TEST_PROGRESS_BYTES;
    unaligned_write = &write_buff[1];

    fres = f_open(&file, FATFS_TEST_PATTERN_FILE, FA_CREATE_ALWAYS | FA_WRITE);
    if (FatFs_CheckResult(fres, "f_open pattern file for write") != 0U)
    {
        return 1U;
    }

    while (offset < FATFS_TEST_PATTERN_BYTES)
    {
        remaining = FATFS_TEST_PATTERN_BYTES - offset;
        chunk = (remaining > FATFS_TEST_CHUNK_BYTES) ?
                FATFS_TEST_CHUNK_BYTES : (UINT)remaining;

        FatFs_FillPattern(unaligned_write, chunk, offset);
        bytes_written = 0U;

        fres = f_write(&file, unaligned_write, chunk, &bytes_written);
        if ((fres != FR_OK) || (bytes_written != chunk))
        {
            printf("[FatFs][ERROR] Pattern write failed at offset %lu. FRESULT=%d, bytes=%u/%u\r\n",
                   offset, fres, bytes_written, chunk);
            failed = 1U;
            break;
        }

        offset += (DWORD)bytes_written;
        if (offset >= next_progress)
        {
            printf("[FatFs][INFO] Written %lu / %lu bytes\r\n",
                   offset, FATFS_TEST_PATTERN_BYTES);
            next_progress += FATFS_TEST_PROGRESS_BYTES;
        }
    }

    if (failed == 0U)
    {
        fres = f_sync(&file);
        if (FatFs_CheckResult(fres, "f_sync pattern file") != 0U)
        {
            failed = 1U;
        }
    }

    fres = f_close(&file);
    if (FatFs_CheckResult(fres, "f_close pattern file after write") != 0U)
    {
        failed = 1U;
    }

    if (failed == 0U)
    {
        printf("[FatFs][PASS] Pattern file write completed.\r\n");
    }

    return failed;
}

static uint8_t FatFs_VerifyPatternFileStatus(void)
{
    FRESULT fres;
    FILINFO info;

    printf("[FatFs][STEP] Checking pattern file metadata.\r\n");

    memset(&info, 0, sizeof(info));
    fres = f_stat(FATFS_TEST_PATTERN_FILE, &info);
    if (FatFs_CheckResult(fres, "f_stat pattern file") != 0U)
    {
        return 1U;
    }

    if (info.fsize != (FSIZE_t)FATFS_TEST_PATTERN_BYTES)
    {
        printf("[FatFs][ERROR] Pattern file size mismatch. actual=%lu expected=%lu\r\n",
               (DWORD)info.fsize, FATFS_TEST_PATTERN_BYTES);
        return 1U;
    }

    printf("[FatFs][PASS] Pattern file size verified: %lu bytes\r\n",
           FATFS_TEST_PATTERN_BYTES);
    return 0U;
}

static uint8_t FatFs_RemountVolume(void)
{
    FRESULT fres;

    printf("[FatFs][STEP] Unmounting and remounting volume.\r\n");

    fres = f_mount(NULL, FATFS_TEST_DRIVE, 0);
    if (FatFs_CheckResult(fres, "f_mount unmount") != 0U)
    {
        return 1U;
    }

    HAL_Delay(50U);

    fres = f_mount(&fs, FATFS_TEST_DRIVE, 1);
    if (FatFs_CheckResult(fres, "f_mount remount") != 0U)
    {
        return 1U;
    }

    printf("[FatFs][PASS] Remount succeeded.\r\n");
    return 0U;
}

static uint8_t FatFs_RunPatternFileReadback(void)
{
    FRESULT fres;
    UINT bytes_read;
    UINT chunk;
    UINT i;
    DWORD offset;
    DWORD remaining;
    DWORD next_progress;
    uint8_t *unaligned_write;
    uint8_t *unaligned_read;
    uint8_t failed;

    printf("[FatFs][STEP] Pattern file readback verification.\r\n");

    failed = 0U;
    offset = 0UL;
    next_progress = FATFS_TEST_PROGRESS_BYTES;
    unaligned_write = &write_buff[1];
    unaligned_read = &read_buff[1];

    fres = f_open(&file, FATFS_TEST_PATTERN_FILE, FA_READ);
    if (FatFs_CheckResult(fres, "f_open pattern file for read") != 0U)
    {
        return 1U;
    }

    while (offset < FATFS_TEST_PATTERN_BYTES)
    {
        remaining = FATFS_TEST_PATTERN_BYTES - offset;
        chunk = (remaining > FATFS_TEST_CHUNK_BYTES) ?
                FATFS_TEST_CHUNK_BYTES : (UINT)remaining;

        memset(unaligned_read, 0, chunk);
        bytes_read = 0U;

        fres = f_read(&file, unaligned_read, chunk, &bytes_read);
        if ((fres != FR_OK) || (bytes_read != chunk))
        {
            printf("[FatFs][ERROR] Pattern read failed at offset %lu. FRESULT=%d, bytes=%u/%u\r\n",
                   offset, fres, bytes_read, chunk);
            failed = 1U;
            break;
        }

        FatFs_FillPattern(unaligned_write, chunk, offset);
        if (memcmp(unaligned_write, unaligned_read, chunk) != 0)
        {
            printf("[FatFs][ERROR] Pattern mismatch near offset %lu\r\n", offset);
            for (i = 0U; i < chunk; i++)
            {
                if (unaligned_write[i] != unaligned_read[i])
                {
                    printf("[FatFs][ERROR] First mismatch at absolute offset %lu: expected=0x%02X actual=0x%02X\r\n",
                           offset + (DWORD)i, unaligned_write[i], unaligned_read[i]);
                    break;
                }
            }
            failed = 1U;
            break;
        }

        offset += (DWORD)bytes_read;
        if (offset >= next_progress)
        {
            printf("[FatFs][INFO] Verified %lu / %lu bytes\r\n",
                   offset, FATFS_TEST_PATTERN_BYTES);
            next_progress += FATFS_TEST_PROGRESS_BYTES;
        }
    }

    fres = f_close(&file);
    if (FatFs_CheckResult(fres, "f_close pattern file after read") != 0U)
    {
        failed = 1U;
    }

    if (failed == 0U)
    {
        printf("[FatFs][PASS] Pattern file readback verified.\r\n");
    }

    return failed;
}

static uint8_t FatFs_RunDirectoryScan(void)
{
    FRESULT fres;
    DIR dir;
    FILINFO info;
    uint8_t found_text;
    uint8_t found_pattern;
    uint8_t failed;

    printf("[FatFs][STEP] Directory scan verification.\r\n");

    found_text = 0U;
    found_pattern = 0U;
    failed = 0U;
    memset(&dir, 0, sizeof(dir));
    memset(&info, 0, sizeof(info));

    fres = f_opendir(&dir, FATFS_TEST_DIR);
    if (FatFs_CheckResult(fres, "f_opendir test directory") != 0U)
    {
        return 1U;
    }

    for (;;)
    {
        fres = f_readdir(&dir, &info);
        if (fres != FR_OK)
        {
            printf("[FatFs][ERROR] f_readdir failed. FRESULT=%d\r\n", fres);
            failed = 1U;
            break;
        }

        if (info.fname[0] == '\0')
        {
            break;
        }

        printf("[FatFs][INFO] Dir entry: %s, size=%lu\r\n",
               info.fname, (DWORD)info.fsize);

        if ((strcmp(info.fname, "test.txt") == 0) ||
            (strcmp(info.fname, "TEST.TXT") == 0))
        {
            found_text = 1U;
        }
        else if ((strcmp(info.fname, "pattern.bin") == 0) ||
                 (strcmp(info.fname, "PATTERN.BIN") == 0))
        {
            found_pattern = 1U;
        }
    }

    fres = f_closedir(&dir);
    if (FatFs_CheckResult(fres, "f_closedir test directory") != 0U)
    {
        failed = 1U;
    }

    if ((found_text == 0U) || (found_pattern == 0U))
    {
        printf("[FatFs][ERROR] Expected test files were not both found. text=%u pattern=%u\r\n",
               found_text, found_pattern);
        failed = 1U;
    }

    if (failed == 0U)
    {
        printf("[FatFs][PASS] Directory scan verified.\r\n");
    }

    return failed;
}

void FatFs_IntegrationTest(void)
{
    FRESULT fres;
    uint32_t fail_count;

    fail_count = 0UL;

    printf("\r\n==========================================\r\n");
    printf("=== FatFs Integration Test Starting ===\r\n");
    printf("==========================================\r\n");

    printf("[FatFs][STEP] Mounting %s\r\n", FATFS_TEST_DRIVE);
    fres = f_mount(&fs, FATFS_TEST_DRIVE, 1);
    if (FatFs_CheckResult(fres, "f_mount") != 0U)
    {
        if (fres == FR_NO_FILESYSTEM)
        {
            printf("[FatFs][INFO] No FAT volume found. Format the SD card on a PC first.\r\n");
        }
        printf("[FatFs][FAIL] Test aborted at mount step.\r\n");
        return;
    }
    printf("[FatFs][PASS] Volume mounted.\r\n");

    if (FatFs_PrintCapacity() != 0U)
    {
        fail_count++;
    }

    if ((fail_count == 0UL) && (FatFs_PrepareTestDirectory() != 0U))
    {
        fail_count++;
    }

    if ((fail_count == 0UL) && (FatFs_RunSmallFileTest() != 0U))
    {
        fail_count++;
    }

    if ((fail_count == 0UL) && (FatFs_RunPatternFileWrite() != 0U))
    {
        fail_count++;
    }

    if ((fail_count == 0UL) && (FatFs_VerifyPatternFileStatus() != 0U))
    {
        fail_count++;
    }

    if ((fail_count == 0UL) && (FatFs_RemountVolume() != 0U))
    {
        fail_count++;
    }

    if ((fail_count == 0UL) && (FatFs_RunPatternFileReadback() != 0U))
    {
        fail_count++;
    }

    if ((fail_count == 0UL) && (FatFs_RunDirectoryScan() != 0U))
    {
        fail_count++;
    }

    fres = f_mount(NULL, FATFS_TEST_DRIVE, 0);
    if (FatFs_CheckResult(fres, "final f_mount unmount") != 0U)
    {
        fail_count++;
    }

    printf("\r\n==========================================\r\n");
    if (fail_count == 0UL)
    {
        printf("=== FatFs Integration Test PASS ===\r\n");
        printf("[FatFs][INFO] Test files are left in %s for PC-side inspection.\r\n",
               FATFS_TEST_DIR);
    }
    else
    {
        printf("=== FatFs Integration Test FAIL ===\r\n");
        printf("[FatFs][INFO] Failure count: %lu\r\n", (unsigned long)fail_count);
    }
    printf("==========================================\r\n");
}
