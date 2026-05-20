#ifndef _BSP_MULTIBUTTON_H_
#define _BSP_MULTIBUTTON_H_

#include "multi_button.h"


enum Button_IDs {
	SW1_id,
	SW2_id
};

void bsp_keyInit(void);
void bsp_key_start(void);
void bsp_key_stop(void);


#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif
