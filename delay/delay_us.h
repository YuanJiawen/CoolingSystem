#ifndef _DELAY_US_H_
#define _DELAY_US_H_

#include "stm32f4xx.h"

void HAL_Delay_us_init(uint8_t SYSCLK);
void HAL_Delay_us(uint32_t nus);


#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif
