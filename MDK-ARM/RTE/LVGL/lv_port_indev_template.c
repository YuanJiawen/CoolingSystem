/**
 * @file lv_port_indev.c
 */

#if 1

/*********************
 * INCLUDES
 *********************/
#include "lv_port_indev_template.h"
#include "lvgl.h"
#include "bsp_touch.h"

/**********************
 * STATIC PROTOTYPES
 **********************/

static void touchpad_init(void);
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);

/**********************
 * STATIC VARIABLES
 **********************/
lv_indev_t * indev_touchpad;

/**********************
 * GLOBAL FUNCTIONS
 **********************/

void lv_port_indev_init(void)
{
  touchpad_init();

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touchpad_read;
  
  indev_drv.scroll_limit = 30;
  /* 光标显示选择*/
  indev_touchpad = lv_indev_drv_register(&indev_drv);
  
  lv_obj_t * mouse_cursor = lv_img_create(lv_layer_sys());
  lv_img_set_src(mouse_cursor, LV_SYMBOL_BULLET);
  lv_indev_set_cursor(indev_touchpad, mouse_cursor);
}

/**********************
 * STATIC FUNCTIONS
 **********************/

/*------------------
 * Touchpad
 * -----------------*/

/*Initialize your touchpad*/
static void touchpad_init(void)
{
  BSP_Touch_Init();
}

/*Will be called by the library to read the touchpad*/

static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{

   static Touch_Data_t touch_data = {0};
   BSP_Touch_Scan(&touch_data);
  
   if(touch_data.is_pressed) {
     data->state = LV_INDEV_STATE_PR;
     data->point.x = touch_data.x;
     data->point.y = touch_data.y;
   } else {
     data->state = LV_INDEV_STATE_REL;
   }
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
