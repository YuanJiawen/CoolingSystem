
#include "bsp_multibutton.h"
#include "main.h"

static struct Button sw1;
static struct Button sw2;
extern uint8_t SwEvent;



static void SW1_PressDownCallback(void* btn){

	SwEvent = 1;

}
static void SW2_PressDownCallback(void* btn){

	SwEvent = 2;

}
static uint8_t read_button_gpio(uint8_t button_id)
{
    switch (button_id) {
			
        case SW1_id:
            return HAL_GPIO_ReadPin(SW1_GPIO_Port,SW1_Pin);
				
				case SW2_id:
						return HAL_GPIO_ReadPin(SW2_GPIO_Port,SW2_Pin);
				
        default:
            return 0;
    }
}

static void keyInit(void){
	
	button_init(&sw1, read_button_gpio, 1, SW1_id);
	button_init(&sw2, read_button_gpio, 1, SW2_id);

}
static void bsp_button_attach(void){

	button_attach(&sw1,PRESS_DOWN,SW1_PressDownCallback);
	button_attach(&sw2,PRESS_DOWN,SW2_PressDownCallback);

}
void bsp_key_start(void){

	button_start(&sw1);
	button_start(&sw2);

}
void bsp_key_stop(void){

	button_stop(&sw1);
	button_stop(&sw2);

}
void bsp_keyInit(void){

	keyInit();
	bsp_button_attach();
	bsp_key_start();

}
