#ifndef __BSP_TOUCH_H
#define __BSP_TOUCH_H

#include "main.h"

/* 定义触摸状态结构体 */
typedef struct
{
    uint16_t x;
    uint16_t y;
    uint8_t  is_pressed; // 1: 按下, 0: 松开
} Touch_Data_t;

/* 对外接口 */
void BSP_Touch_Init(void);
uint8_t BSP_Touch_Scan(Touch_Data_t *pData); // 返回 1 表示获取到有效触摸
uint8_t touch_is_pressed(void);
void BSP_Touch_Get_XY(uint16_t *x, uint16_t *y);

#endif
