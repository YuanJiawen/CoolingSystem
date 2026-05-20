#include "fmc.h"  
#include "bsp_sdram.h"
#include "main.h"

/* SDRAM 模式寄存器定义 */
#define SDRAM_MODEREG_BURST_LENGTH_1             ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_LENGTH_2             ((uint16_t)0x0001)
#define SDRAM_MODEREG_BURST_LENGTH_4             ((uint16_t)0x0002)
#define SDRAM_MODEREG_BURST_LENGTH_8             ((uint16_t)0x0004)
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL      ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_TYPE_INTERLEAVED     ((uint16_t)0x0008)
#define SDRAM_MODEREG_CAS_LATENCY_2              ((uint16_t)0x0020) // CL=2
#define SDRAM_MODEREG_CAS_LATENCY_3              ((uint16_t)0x0030)
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD    ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_PROGRAMMED ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE     ((uint16_t)0x0200)

/**
  * @brief  对 SDRAM Bank 2 进行初始化 (MX_FMC_Init后)
  * @param  hsdram: SDRAM 句柄
  */
void BSP_SDRAM_Init_Sequence(SDRAM_HandleTypeDef *hsdram)
{
  FMC_SDRAM_CommandTypeDef Command;
  __IO uint32_t tmpmrd = 0;

  /* 1. 开启时钟 (Clock Enable) */
  Command.CommandMode            = FMC_SDRAM_CMD_CLK_ENABLE;
  Command.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK2; // Bank 2
  Command.AutoRefreshNumber      = 1;
  Command.ModeRegisterDefinition = 0;
  HAL_SDRAM_SendCommand(hsdram, &Command, 0x1000);

  /* 延时 > 100us */
  HAL_Delay(2);

  /* 2. Precharge所有 Bank */
  Command.CommandMode            = FMC_SDRAM_CMD_PALL;
  Command.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK2; // Bank 2
  Command.AutoRefreshNumber      = 1;
  Command.ModeRegisterDefinition = 0;
  HAL_SDRAM_SendCommand(hsdram, &Command, 0x1000);

  /* 3. 自动刷新 (Auto Refresh) - 发送 8 次 */
  Command.CommandMode            = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
  Command.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK2; // Bank 2
  Command.AutoRefreshNumber      = 8;
  Command.ModeRegisterDefinition = 0;
  HAL_SDRAM_SendCommand(hsdram, &Command, 0x1000);

  /* 4. 加载模式寄存器 (Load Mode Register) */
  /* 配置: Burst Length=1, CAS Latency=2, Single Write */
  tmpmrd = (uint32_t)(SDRAM_MODEREG_BURST_LENGTH_1          |
                      SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL   |
                      SDRAM_MODEREG_CAS_LATENCY_2           | // CL=2
                      SDRAM_MODEREG_OPERATING_MODE_STANDARD |
                      SDRAM_MODEREG_WRITEBURST_MODE_PROGRAMMED);

  Command.CommandMode            = FMC_SDRAM_CMD_LOAD_MODE;
  Command.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK2; // Bank 2
  Command.AutoRefreshNumber      = 1;
  Command.ModeRegisterDefinition = tmpmrd;
  HAL_SDRAM_SendCommand(hsdram, &Command, 0x1000);

  /* 5. 设置刷新率计数器 */
  /* W9825G6: 64ms / 8192 rows -> ~7.8us per row */
  /* Count = 7.8us * 90MHz - 20 = 682 */
  HAL_SDRAM_ProgramRefreshRate(hsdram, 682);
}