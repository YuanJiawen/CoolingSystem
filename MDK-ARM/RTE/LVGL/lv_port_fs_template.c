/**
 * @file lv_port_fs_template.c
 * @brief LVGL file-system port backed by FatFS.
 */

#include "lv_port_fs_template.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>

#define LV_FATFS_LETTER          'S'
#define LV_FATFS_DRIVE_PREFIX    "0:"
#define LV_FATFS_PATH_MAX        260U

static FATFS sd_fs;
static uint8_t sd_fs_mounted = 0U;

static lv_fs_res_t fatfs_to_lv_res(FRESULT res);
static void make_fatfs_path(char *out, uint32_t out_size, const char *path);

static void * fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode);
static lv_fs_res_t fs_close(lv_fs_drv_t * drv, void * file_p);
static lv_fs_res_t fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br);
static lv_fs_res_t fs_write(lv_fs_drv_t * drv, void * file_p, const void * buf, uint32_t btw, uint32_t * bw);
static lv_fs_res_t fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence);
static lv_fs_res_t fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p);
static void * fs_dir_open(lv_fs_drv_t * drv, const char * path);
static lv_fs_res_t fs_dir_read(lv_fs_drv_t * drv, void * rddir_p, char * fn);
static lv_fs_res_t fs_dir_close(lv_fs_drv_t * drv, void * rddir_p);

void lv_port_fs_init(void)
{
    FRESULT fres;
    static lv_fs_drv_t fs_drv;

    fres = f_mount(&sd_fs, LV_FATFS_DRIVE_PREFIX, 1);
    if(fres == FR_OK) {
        sd_fs_mounted = 1U;
    } else {
        sd_fs_mounted = 0U;
        printf("[lv_fs][ERROR] f_mount failed. FRESULT=%d\r\n", fres);
    }

    lv_fs_drv_init(&fs_drv);
    fs_drv.letter = LV_FATFS_LETTER;
    fs_drv.open_cb = fs_open;
    fs_drv.close_cb = fs_close;
    fs_drv.read_cb = fs_read;
    fs_drv.write_cb = fs_write;
    fs_drv.seek_cb = fs_seek;
    fs_drv.tell_cb = fs_tell;
    fs_drv.dir_open_cb = fs_dir_open;
    fs_drv.dir_read_cb = fs_dir_read;
    fs_drv.dir_close_cb = fs_dir_close;

    lv_fs_drv_register(&fs_drv);
}

static lv_fs_res_t fatfs_to_lv_res(FRESULT res)
{
    switch(res) {
        case FR_OK:
            return LV_FS_RES_OK;
        case FR_NO_FILE:
        case FR_NO_PATH:
        case FR_INVALID_NAME:
            return LV_FS_RES_NOT_EX;
        case FR_DENIED:
        case FR_WRITE_PROTECTED:
            return LV_FS_RES_DENIED;
        case FR_NOT_READY:
            return LV_FS_RES_HW_ERR;
        case FR_TIMEOUT:
            return LV_FS_RES_TOUT;
        case FR_NOT_ENOUGH_CORE:
            return LV_FS_RES_OUT_OF_MEM;
        case FR_INVALID_OBJECT:
            return LV_FS_RES_INV_PARAM;
        default:
            return LV_FS_RES_UNKNOWN;
    }
}

static void make_fatfs_path(char *out, uint32_t out_size, const char *path)
{
    const char *src = path;

    if((out == NULL) || (out_size == 0U)) {
        return;
    }

    if(src == NULL) {
        src = "";
    }

    if(src[0] == '/') {
        (void)snprintf(out, out_size, "%s%s", LV_FATFS_DRIVE_PREFIX, src);
    } else {
        (void)snprintf(out, out_size, "%s/%s", LV_FATFS_DRIVE_PREFIX, src);
    }
}

static void * fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode)
{
    FIL *file;
    BYTE fatfs_mode = 0U;
    FRESULT fres;
    char fatfs_path[LV_FATFS_PATH_MAX];

    (void)drv;

    if(sd_fs_mounted == 0U) {
        return NULL;
    }

    if((mode & LV_FS_MODE_RD) != 0U) {
        fatfs_mode |= FA_READ;
    }
    if((mode & LV_FS_MODE_WR) != 0U) {
        fatfs_mode |= (FA_WRITE | FA_OPEN_ALWAYS);
    }

    file = (FIL *)lv_mem_alloc(sizeof(FIL));
    if(file == NULL) {
        return NULL;
    }

    make_fatfs_path(fatfs_path, sizeof(fatfs_path), path);
    fres = f_open(file, fatfs_path, fatfs_mode);
    if(fres != FR_OK) {
        printf("[lv_fs][ERROR] f_open %s failed. FRESULT=%d\r\n", fatfs_path, fres);
        lv_mem_free(file);
        return NULL;
    }

    return file;
}

static lv_fs_res_t fs_close(lv_fs_drv_t * drv, void * file_p)
{
    FRESULT fres;

    (void)drv;

    if(file_p == NULL) {
        return LV_FS_RES_INV_PARAM;
    }

    fres = f_close((FIL *)file_p);
    lv_mem_free(file_p);
    return fatfs_to_lv_res(fres);
}

static lv_fs_res_t fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br)
{
    FRESULT fres;
    UINT bytes_read = 0U;

    (void)drv;

    if((file_p == NULL) || (buf == NULL)) {
        return LV_FS_RES_INV_PARAM;
    }

    fres = f_read((FIL *)file_p, buf, (UINT)btr, &bytes_read);
    if(br != NULL) {
        *br = bytes_read;
    }

    return fatfs_to_lv_res(fres);
}

static lv_fs_res_t fs_write(lv_fs_drv_t * drv, void * file_p, const void * buf, uint32_t btw, uint32_t * bw)
{
    FRESULT fres;
    UINT bytes_written = 0U;

    (void)drv;

    if((file_p == NULL) || (buf == NULL)) {
        return LV_FS_RES_INV_PARAM;
    }

    fres = f_write((FIL *)file_p, buf, (UINT)btw, &bytes_written);
    if(bw != NULL) {
        *bw = bytes_written;
    }

    return fatfs_to_lv_res(fres);
}

static lv_fs_res_t fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence)
{
    FIL *file = (FIL *)file_p;
    FSIZE_t base = 0U;
    FSIZE_t target;

    (void)drv;

    if(file == NULL) {
        return LV_FS_RES_INV_PARAM;
    }

    if(whence == LV_FS_SEEK_CUR) {
        base = f_tell(file);
    } else if(whence == LV_FS_SEEK_END) {
        base = f_size(file);
    }

    target = base + (FSIZE_t)pos;
    return fatfs_to_lv_res(f_lseek(file, target));
}

static lv_fs_res_t fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p)
{
    (void)drv;

    if((file_p == NULL) || (pos_p == NULL)) {
        return LV_FS_RES_INV_PARAM;
    }

    *pos_p = (uint32_t)f_tell((FIL *)file_p);
    return LV_FS_RES_OK;
}

static void * fs_dir_open(lv_fs_drv_t * drv, const char * path)
{
    DIR *dir;
    FRESULT fres;
    char fatfs_path[LV_FATFS_PATH_MAX];

    (void)drv;

    if(sd_fs_mounted == 0U) {
        return NULL;
    }

    dir = (DIR *)lv_mem_alloc(sizeof(DIR));
    if(dir == NULL) {
        return NULL;
    }

    make_fatfs_path(fatfs_path, sizeof(fatfs_path), path);
    fres = f_opendir(dir, fatfs_path);
    if(fres != FR_OK) {
        printf("[lv_fs][ERROR] f_opendir %s failed. FRESULT=%d\r\n", fatfs_path, fres);
        lv_mem_free(dir);
        return NULL;
    }

    return dir;
}

static lv_fs_res_t fs_dir_read(lv_fs_drv_t * drv, void * rddir_p, char * fn)
{
    FRESULT fres;
    FILINFO info;

    (void)drv;

    if((rddir_p == NULL) || (fn == NULL)) {
        return LV_FS_RES_INV_PARAM;
    }

    fres = f_readdir((DIR *)rddir_p, &info);
    if(fres != FR_OK) {
        return fatfs_to_lv_res(fres);
    }

    if(info.fname[0] == '\0') {
        fn[0] = '\0';
    } else if((info.fattrib & AM_DIR) != 0U) {
        fn[0] = '/';
        (void)strncpy(&fn[1], info.fname, LV_FS_MAX_PATH_LENGTH - 2U);
        fn[LV_FS_MAX_PATH_LENGTH - 1U] = '\0';
    } else {
        (void)strncpy(fn, info.fname, LV_FS_MAX_PATH_LENGTH - 1U);
        fn[LV_FS_MAX_PATH_LENGTH - 1U] = '\0';
    }

    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_dir_close(lv_fs_drv_t * drv, void * rddir_p)
{
    FRESULT fres;

    (void)drv;

    if(rddir_p == NULL) {
        return LV_FS_RES_INV_PARAM;
    }

    fres = f_closedir((DIR *)rddir_p);
    lv_mem_free(rddir_p);
    return fatfs_to_lv_res(fres);
}
