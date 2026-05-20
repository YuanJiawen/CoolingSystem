/*-----------------------------------------------------------------------*/
/* Low level disk interface module include file                          */
/*-----------------------------------------------------------------------*/

#ifndef _DISKIO_DEFINED
#define _DISKIO_DEFINED

#ifdef __cplusplus
extern "C" {
#endif

#include "ff.h"

/* Disk Status Bits */
typedef BYTE DSTATUS;

#define STA_NOINIT      0x01    /* Drive not initialized */
#define STA_NODISK      0x02    /* No medium in the drive */
#define STA_PROTECT     0x04    /* Write protected */

/* Results of Disk Functions */
typedef enum {
    RES_OK = 0,
    RES_ERROR,
    RES_WRPRT,
    RES_NOTRDY,
    RES_PARERR
} DRESULT;

/* Physical drive number */
#define DEV_SD          0

/* Disk function prototypes */
DSTATUS disk_initialize(BYTE pdrv);
DSTATUS disk_status(BYTE pdrv);
DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count);
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count);
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff);
DWORD get_fattime(void);

/* Optional debug helper */
uint32_t diskio_sd_last_error(void);

/* Generic command */
#define CTRL_SYNC           0
#define GET_SECTOR_COUNT    1
#define GET_SECTOR_SIZE     2
#define GET_BLOCK_SIZE      3
#define CTRL_TRIM           4

#ifdef __cplusplus
}
#endif

#endif /* _DISKIO_DEFINED */