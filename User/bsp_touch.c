#include "bsp_touch.h"
#include "i2c.h" 
#include "gpio.h"

/* ========================================================================= */
/* 【I2C 模式选择】                                                       */
/* ========================================================================= */
/* 1: 使用 DMA 非阻塞读取 (需配置 I2C2 RX DMA 并开启 I2C 中断)
 * 0: 使用普通轮询阻塞读取 (不依赖 DMA )
 */
#define USE_TOUCH_I2C_DMA  0  /* 修改 0 或 1 进行切换 */
/* ========================================================================= */

/* GT1151Q 地址 */
#define GT_ADDR_WR      0x28
#define GT_ADDR_RD      0x29

/* 寄存器定义 */
#define GT_REG_CMD      0x8040
#define GT_REG_STATUS   0x814E
#define GT_REG_COORD    0x8150

/* I2C 句柄 */
extern I2C_HandleTypeDef hi2c2;

#if USE_TOUCH_I2C_DMA == 1
    /* DMA 接收缓冲区和状态标志 */
    static uint8_t touch_dma_buf[8]; 
    static volatile uint8_t i2c_dma_busy = 0; 
#endif

static uint16_t g_last_x = 0;
static uint16_t g_last_y = 0;
static uint8_t  g_last_pressed = 0;

/* 复位 */
static void GT_Reset_Sequence(void)
{
		
    GPIO_InitTypeDef GPIO_InitStruct = {0};	
		
		__HAL_RCC_GPIOD_CLK_ENABLE();
		
    // 1. 配置 INT 为推挽输出，拉低
    GPIO_InitStruct.Pin = TOUCH_INT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(TOUCH_INT_GPIO_Port, &GPIO_InitStruct);
		
    // 2. RST=0, INT=0 (复位并选择地址 0x28)
    HAL_GPIO_WritePin(TOUCH_RST_GPIO_Port, TOUCH_RST_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TOUCH_INT_GPIO_Port, TOUCH_INT_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);

    // 3. RST=1 (释放复位)
    HAL_GPIO_WritePin(TOUCH_RST_GPIO_Port, TOUCH_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(5);

    // 4. 将 INT 恢复为浮空输入
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT; 
    HAL_GPIO_Init(TOUCH_INT_GPIO_Port, &GPIO_InitStruct);

    HAL_Delay(5); // 等待芯片启动
}

/* 初始化函数 */
void BSP_Touch_Init(void)
{
    GT_Reset_Sequence();
}

#if USE_TOUCH_I2C_DMA == 1
/* DMA 传输完成回调 */
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C2)
    {
        i2c_dma_busy = 0; // 标记 DMA 空闲
    }
}

/* 当 I2C 发生错误时（包括 DMA 无法访问等硬件错误），释放 Busy 标志 */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C2)
    {
        i2c_dma_busy = 0; 
    }
}
#endif

/* 扫描函数：负责更新缓存 */
uint8_t BSP_Touch_Scan(Touch_Data_t *pData)
{
#if USE_TOUCH_I2C_DMA == 1
    /* ========================================== */
    /* 模式 1：DMA 非阻塞读取逻辑 (适合 LVGL)      */
    /* ========================================== */
    static uint8_t step = 0;
    
    if (i2c_dma_busy) return 0;

    if (step == 0)
    {
        i2c_dma_busy = 1;
        /* 发起 DMA 读取 */
        if (HAL_I2C_Mem_Read_DMA(&hi2c2, GT_ADDR_WR, GT_REG_STATUS, 
                                 I2C_MEMADD_SIZE_16BIT, touch_dma_buf, 7) != HAL_OK)
        {
            i2c_dma_busy = 0;
            return 0;
        }
        step = 1;
        return 0;
    }
    else if (step == 1)
    {
        uint8_t status = touch_dma_buf[0];
        
        if (status == 0xFF) 
        {
            step = 0; 
            return 0; 
        }

        /* 检查 Buffer Status (0x80) 和 触摸点数 */
        if ((status & 0x80) && (status & 0x0F) > 0)
        {
            uint16_t x = (touch_dma_buf[3] << 8) | touch_dma_buf[2];
            uint16_t y = (touch_dma_buf[5] << 8) | touch_dma_buf[4];
            
            if(pData) {
                pData->x = x;
                pData->y = y;
                pData->is_pressed = 1;
            }
            
            /* 更新静态缓存 */
            g_last_x = x;
            g_last_y = y;
            g_last_pressed = 1;

            /* 清除状态位 */
            uint8_t clear = 0;
            HAL_I2C_Mem_Write(&hi2c2, GT_ADDR_WR, GT_REG_STATUS, I2C_MEMADD_SIZE_16BIT, &clear, 1, 100);
            
            step = 0;
            return 1;
        }
        else
        {
            /* 没按或者数据无效 */
            if (status & 0x80)
            {
                uint8_t clear = 0;
                HAL_I2C_Mem_Write(&hi2c2, GT_ADDR_WR, GT_REG_STATUS, I2C_MEMADD_SIZE_16BIT, &clear, 1, 100);
            }
            
            if(pData) pData->is_pressed = 0;
            g_last_pressed = 0;
        }
        step = 0;
    }
    return 0;

#else
    /* ========================================== */
    /* 模式 0：普通轮询阻塞读取逻辑 (简单稳定)     */
    /* ========================================== */
    uint8_t buf[8];
    
    /* 阻塞式读取 7 个字节 */
    if (HAL_I2C_Mem_Read(&hi2c2, GT_ADDR_WR, GT_REG_STATUS, 
                         I2C_MEMADD_SIZE_16BIT, buf, 7, 100) != HAL_OK)
    {
        return 0;
    }
    
    uint8_t status = buf[0];
    if (status == 0xFF) return 0;
    
    /* 检查 Buffer Status (0x80) 和 触摸点数 */
    if ((status & 0x80) && (status & 0x0F) > 0)
    {
        uint16_t x = (buf[3] << 8) | buf[2];
        uint16_t y = (buf[5] << 8) | buf[4];
        
        if(pData) {
            pData->x = x;
            pData->y = y;
            pData->is_pressed = 1;
        }
        
        g_last_x = x;
        g_last_y = y;
        g_last_pressed = 1;

        /* 清除状态位 */
        uint8_t clear = 0;
        HAL_I2C_Mem_Write(&hi2c2, GT_ADDR_WR, GT_REG_STATUS, I2C_MEMADD_SIZE_16BIT, &clear, 1, 100);
        return 1;
    }
    else
    {
        if (status & 0x80)
        {
            uint8_t clear = 0;
            HAL_I2C_Mem_Write(&hi2c2, GT_ADDR_WR, GT_REG_STATUS, I2C_MEMADD_SIZE_16BIT, &clear, 1, 100);
        }
        
        if(pData) pData->is_pressed = 0;
        g_last_pressed = 0;
    }
    return 0;
#endif
}

/* ============================= */
/* LVGL 接口函数         */
/* ============================= */

/**
 * @brief  通知LVGL 当前是否有手指按下
 * @return 1: 按下, 0: 松开
 */
uint8_t touch_is_pressed(void)
{
    return g_last_pressed;
}

/**
 * @brief  获取当前的物理坐标 
 * @param  x: 返回的 X 坐标指针
 * @param  y: 返回的 Y 坐标指针
 */
void BSP_Touch_Get_XY(uint16_t *x, uint16_t *y)
{
    /* 读取原始坐标 */
    uint16_t raw_x = g_last_x;
    uint16_t raw_y = g_last_y;

    // 方案 1: 如果屏幕是横屏，请尝试交换 X/Y (解除注释)
     //*x = raw_y;
     //*y = raw_x; // 如果方向反了，试试 *y = 480 - raw_x;

    // 方案 2: 如果上面的不对试试这个
     *x = raw_x;
     *y = raw_y;
}