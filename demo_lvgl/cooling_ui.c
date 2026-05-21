/**
 * @file cooling_ui.c
 * @brief 冷却系统实时监控面板 UI 实现 (修复指针残留与白屏 Bug)
 * MCU: STM32F429IG | LVGL 8.x | 800x480 RGB
 */

#include "cooling_ui.h"
#include <stdio.h>

/* ==================== 外部字体声明 ==================== */
LV_FONT_DECLARE(my_font_chinese_16);
LV_FONT_DECLARE(my_font_chinese_18);
LV_FONT_DECLARE(my_font_chinese_20);
LV_FONT_DECLARE(my_font_chinese_22);
LV_FONT_DECLARE(my_font_chinese_24);

/* ==================== 颜色宏定义 ==================== */
#define COLOR_BG_DARK        lv_color_hex(0xF4F8FB)
#define COLOR_CARD_BG        lv_color_hex(0xFFFFFF)
#define COLOR_TITLE_BAR      lv_color_hex(0xE8F4FB)
#define COLOR_TEXT_WHITE     lv_color_hex(0x17324D)
#define COLOR_TEXT_LIGHT     lv_color_hex(0x5C7184)
#define COLOR_BORDER_LIGHT   lv_color_hex(0xD6E2EA)
#define COLOR_PANEL_BG       lv_color_hex(0xF9FCFE)
#define COLOR_GREEN          lv_color_hex(0x4CAF50)   
#define COLOR_RED            lv_color_hex(0xF44336)   
#define COLOR_YELLOW         lv_color_hex(0xFFC107)   
#define COLOR_ORANGE         lv_color_hex(0xFF9800)   
#define COLOR_BLUE_ACCENT    lv_color_hex(0x1976D2)

/* ==================== 静态控件句柄 ==================== */
/* 核心修复 1：增加独立的 meter 句柄，精准控制指针刷新区域 */
static lv_obj_t *tank_meter;
static lv_obj_t *tank_value_label;
static lv_meter_indicator_t *tank_indic;

static lv_obj_t *pipe_meter;
static lv_obj_t *pipe_value_label;
static lv_meter_indicator_t *pipe_indic;

static lv_obj_t *coolant_level_label;
static lv_obj_t *coolant_level_led;

static lv_obj_t *tank_conn_label;
static lv_obj_t *tank_conn_led;  

static lv_obj_t *valve_led;
static lv_obj_t *valve_label;

static lv_obj_t *heater_led;
static lv_obj_t *heater_label;

static lv_obj_t *sd_logo_img;

static lv_obj_t *warning_overlay;
static lv_obj_t *warning_cont;  
static lv_obj_t *warning_icon;
static lv_obj_t *warning_text;

/* 核心修复 2：使用定时器替代容易触发渲染 Bug 的透明度动画 */
static lv_timer_t *warning_timer;  

/* ==================== 内部工具函数 ==================== */

/* 工业级硬闪烁定时器回调：直接切换容器的隐藏/显示标志位 */
static void warning_blink_cb(lv_timer_t * timer)
{
    (void)timer;
    if(lv_obj_has_flag(warning_cont, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(warning_cont, LV_OBJ_FLAG_HIDDEN); /* 显示 */
    } else {
        lv_obj_add_flag(warning_cont, LV_OBJ_FLAG_HIDDEN);   /* 隐藏 */
    }
}

/* 仪表盘创建：增加了 out_meter 参数将核心对象传出 */
static void create_gauge_meter(lv_obj_t *parent, int32_t x, int32_t y,
                               lv_obj_t **out_meter,
                               lv_meter_indicator_t **out_indic, 
                               lv_obj_t **out_val_label, 
                               const char *title_text)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 200, 270);
    lv_obj_set_pos(cont, x, y);
    lv_obj_set_style_bg_color(cont, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(cont, COLOR_BORDER_LIGHT, 0);
    lv_obj_set_style_border_width(cont, 1, 0);
    lv_obj_set_style_radius(cont, 12, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_lbl = lv_label_create(cont);
    lv_obj_set_style_text_font(title_lbl, &my_font_chinese_18, 0);
    lv_obj_set_style_text_color(title_lbl, COLOR_TEXT_LIGHT, 0);
    lv_label_set_text(title_lbl, title_text);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_MID, 0, 5);

    lv_obj_t *meter = lv_meter_create(cont);
    lv_obj_set_size(meter, 180, 180);
    lv_obj_align(meter, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_clear_flag(meter, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(meter, LV_OPA_TRANSP, 0); 
    lv_obj_set_style_border_width(meter, 0, 0);
    lv_obj_set_style_text_color(meter, COLOR_TEXT_WHITE, LV_PART_TICKS);
    *out_meter = meter; /* 传出 meter 句柄，用于后续消除拖影 */

    lv_meter_scale_t * scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_ticks(meter, scale, 31, 2, 10, lv_color_hex(0xA8B8C5));
    lv_meter_set_scale_major_ticks(meter, scale, 6, 4, 15, COLOR_TEXT_WHITE, 12);
    lv_meter_set_scale_range(meter, scale, 0, 150, 270, 135);

    lv_meter_indicator_t * arc_yellow = lv_meter_add_arc(meter, scale, 10, COLOR_YELLOW, 0);
    lv_meter_set_indicator_start_value(meter, arc_yellow, 0);
    lv_meter_set_indicator_end_value(meter, arc_yellow, 80);

    lv_meter_indicator_t * arc_green = lv_meter_add_arc(meter, scale, 10, COLOR_GREEN, 0);
    lv_meter_set_indicator_start_value(meter, arc_green, 80);
    lv_meter_set_indicator_end_value(meter, arc_green, 120);

    lv_meter_indicator_t * arc_red = lv_meter_add_arc(meter, scale, 10, COLOR_RED, 0);
    lv_meter_set_indicator_start_value(meter, arc_red, 120);
    lv_meter_set_indicator_end_value(meter, arc_red, 150);

    *out_indic = lv_meter_add_needle_line(meter, scale, 4, COLOR_TEXT_WHITE, -10);

    *out_val_label = lv_label_create(cont);
    lv_obj_set_style_text_font(*out_val_label, &my_font_chinese_22, 0);
    lv_obj_set_style_text_color(*out_val_label, COLOR_TEXT_WHITE, 0);
    lv_label_set_text(*out_val_label, "0.0 PSI");
    lv_obj_align(*out_val_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

static lv_obj_t* create_status_row(lv_obj_t *parent, int32_t y_offset,
                                    const char *title_text, const char *val_text,
                                    lv_obj_t **led_out, lv_obj_t **label_out)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, 280, 50);
    lv_obj_set_style_bg_color(row, COLOR_CARD_BG, 0);
    lv_obj_set_style_border_color(row, COLOR_BORDER_LIGHT, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_radius(row, 8, 0);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y_offset);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(row);
    lv_obj_set_style_text_font(title, &my_font_chinese_18, 0);
    lv_obj_set_style_text_color(title, COLOR_BLUE_ACCENT, 0);
    lv_label_set_text(title, title_text);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);

    if (led_out != NULL) {
        *led_out = lv_led_create(row);
        lv_obj_set_size(*led_out, 16, 16);
        lv_obj_align(*led_out, LV_ALIGN_LEFT_MID, 130, 0);
        lv_led_set_brightness(*led_out, 255);
    }

    *label_out = lv_label_create(row);
    lv_obj_set_style_text_font(*label_out, &my_font_chinese_18, 0);
    lv_obj_set_style_text_color(*label_out, COLOR_TEXT_WHITE, 0);
    lv_label_set_text(*label_out, val_text);
    lv_obj_align(*label_out, LV_ALIGN_LEFT_MID, 160, 0);

    return row;
}

/* ==================== 对外接口实现 ==================== */

void cooling_ui_create(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, COLOR_BG_DARK, 0);

    /* ================= 1. 顶部标题栏 ================= */
    lv_obj_t *title_bar = lv_obj_create(scr);
    lv_obj_set_size(title_bar, 800, 50);
    lv_obj_set_style_bg_color(title_bar, COLOR_TITLE_BAR, 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_cont = lv_obj_create(title_bar);
    lv_obj_set_style_bg_opa(title_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(title_cont, 0, 0);
    lv_obj_set_size(title_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(title_cont, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_layout(title_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(title_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(title_cont, 0, 0);
    lv_obj_clear_flag(title_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_lbl = lv_label_create(title_cont);
    lv_obj_set_style_text_font(title_lbl, &my_font_chinese_24, 0);
    lv_obj_set_style_text_color(title_lbl, COLOR_TEXT_WHITE, 0);
    lv_label_set_text(title_lbl, "冷却系统实时监控面板");

    /* ================= 2. 左侧仪表盘 ================= */
    create_gauge_meter(scr, 20, 70, &tank_meter, &tank_indic, &tank_value_label, "冷却罐压力");
    create_gauge_meter(scr, 250, 70, &pipe_meter, &pipe_indic, &pipe_value_label, "管路压力");

    /* ================= 3. 右侧状态面板 ================= */
    lv_obj_t *status_panel = lv_obj_create(scr);
    lv_obj_set_size(status_panel, 300, 360);
    lv_obj_set_pos(status_panel, 480, 70);
    lv_obj_set_style_bg_color(status_panel, COLOR_PANEL_BG, 0);
    lv_obj_set_style_border_color(status_panel, COLOR_BORDER_LIGHT, 0);
    lv_obj_clear_flag(status_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel_title_cont = lv_obj_create(status_panel);
    lv_obj_set_style_bg_opa(panel_title_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(panel_title_cont, 0, 0);
    lv_obj_set_size(panel_title_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(panel_title_cont, LV_ALIGN_TOP_MID, 0, 5); 
    lv_obj_set_layout(panel_title_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel_title_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(panel_title_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(panel_title_cont, 0, 0); 
    lv_obj_clear_flag(panel_title_cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel_title = lv_label_create(panel_title_cont);
    lv_obj_set_style_text_font(panel_title, &my_font_chinese_20, 0);
    lv_obj_set_style_text_color(panel_title, COLOR_BLUE_ACCENT, 0);
    lv_label_set_text(panel_title, "设备状态");

    create_status_row(status_panel, 50, "冷却剂", "充足", &coolant_level_led, &coolant_level_label);
    
    create_status_row(status_panel, 110, "压力罐", "未连接", &tank_conn_led, &tank_conn_label);
    lv_led_set_color(tank_conn_led, COLOR_RED);
    lv_led_on(tank_conn_led);

    create_status_row(status_panel, 170, "电磁阀", "关闭", &valve_led, &valve_label);
    create_status_row(status_panel, 230, "加热片", "未开启", &heater_led, &heater_label);

    lv_obj_t *footer_warn = lv_label_create(scr);
    lv_obj_set_style_text_font(footer_warn, &my_font_chinese_16, 0);
    lv_obj_set_style_text_color(footer_warn, COLOR_RED, 0);
    lv_label_set_text(footer_warn, "存在高压 非专业人员请勿操作该设备！");
    lv_obj_align(footer_warn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    sd_logo_img = lv_img_create(scr);
    lv_img_set_src(sd_logo_img, "S:/IconDir/spintech_icon_120x50.png");
    lv_obj_align(sd_logo_img, LV_ALIGN_TOP_RIGHT, -12, 0);

    /* ================= 4. 全屏警告遮罩 ================= */
    warning_overlay = lv_obj_create(scr);
    lv_obj_set_size(warning_overlay, 800, 480);
    lv_obj_set_style_bg_color(warning_overlay, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(warning_overlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(warning_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(warning_overlay, LV_OBJ_FLAG_HIDDEN);

    warning_cont = lv_obj_create(warning_overlay);
    lv_obj_set_style_bg_opa(warning_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(warning_cont, 0, 0);
    lv_obj_set_size(warning_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(warning_cont, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_layout(warning_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(warning_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(warning_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(warning_cont, 15, 0);
    lv_obj_set_style_pad_all(warning_cont, 0, 0);
    lv_obj_clear_flag(warning_cont, LV_OBJ_FLAG_SCROLLABLE);

    warning_icon = lv_label_create(warning_cont);
    lv_label_set_text(warning_icon, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_font(warning_icon, lv_theme_get_font_large(scr), 0);
    lv_obj_set_style_text_color(warning_icon, COLOR_YELLOW, 0);

    warning_text = lv_label_create(warning_cont);
    lv_label_set_text(warning_text, "压力过大 请联系专业人员！");
    lv_obj_set_style_text_font(warning_text, &my_font_chinese_24, 0);
    lv_obj_set_style_text_color(warning_text, COLOR_RED, 0);

    /* 核心修复 2：初始化定时器并立即暂停，等待超压时唤醒 */
    warning_timer = lv_timer_create(warning_blink_cb, 500, NULL);
    lv_timer_pause(warning_timer);
}

/* -------------- 接口更新 -------------- */
void cooling_ui_set_tank_pressure(float pressure)
{
    if (pressure < 0) pressure = 0;
    if (pressure > 150) pressure = 150;
    
    int p_int = (int)pressure;
    int p_frac = (int)((pressure - (float)p_int) * 10);

    /* 传入正确的 tank_meter 句柄 */
    lv_meter_set_indicator_value(tank_meter, tank_indic, (int32_t)pressure);
    lv_label_set_text_fmt(tank_value_label, "%d.%d PSI", p_int, p_frac);
    
    /* 核心修复 1：强制重绘整个仪表盘，彻底消灭由于抗锯齿溢出带来的指针拖影 */
    lv_obj_invalidate(tank_meter);
}

void cooling_ui_set_pipe_pressure(float pressure)
{
    if (pressure < 0) pressure = 0;
    if (pressure > 150) pressure = 150;

    int p_int = (int)pressure;
    int p_frac = (int)((pressure - (float)p_int) * 10);

    lv_meter_set_indicator_value(pipe_meter, pipe_indic, (int32_t)pressure);
    lv_label_set_text_fmt(pipe_value_label, "%d.%d PSI", p_int, p_frac);
    
    /* 核心修复 1：强制重绘整个仪表盘 */
    lv_obj_invalidate(pipe_meter);
}

void cooling_ui_show_overpressure_warning(void)
{
    lv_obj_clear_flag(warning_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(warning_cont, LV_OBJ_FLAG_HIDDEN); /* 确保初始可见 */
    lv_obj_move_foreground(warning_overlay);
    
    /* 启动定时器硬闪烁 */
    lv_timer_resume(warning_timer);
}

void cooling_ui_hide_overpressure_warning(void)
{
    lv_obj_add_flag(warning_overlay, LV_OBJ_FLAG_HIDDEN);
    
    /* 暂停定时器闪烁 */
    lv_timer_pause(warning_timer);
}

void cooling_ui_set_coolant_level(coolant_level_t level)
{
    switch (level) {
        case COOLANT_LEVEL_FULL:
            lv_label_set_text(coolant_level_label, "充足");
            lv_obj_set_style_text_color(coolant_level_label, COLOR_GREEN, 0);
            lv_led_set_color(coolant_level_led, COLOR_GREEN);
            break;
        case COOLANT_LEVEL_OK:
            lv_label_set_text(coolant_level_label, "尚可");
            lv_obj_set_style_text_color(coolant_level_label, COLOR_YELLOW, 0);
            lv_led_set_color(coolant_level_led, COLOR_YELLOW);
            break;
        case COOLANT_LEVEL_EMPTY:
            lv_label_set_text(coolant_level_label, "耗尽");
            lv_obj_set_style_text_color(coolant_level_label, COLOR_RED, 0);
            lv_led_set_color(coolant_level_led, COLOR_RED);
            break;
    }
}

void cooling_ui_set_tank_connection(tank_conn_state_t state)
{
    switch (state) {
        case TANK_CONNECTED:
            lv_label_set_text(tank_conn_label, "已连接");
            lv_obj_set_style_text_color(tank_conn_label, COLOR_GREEN, 0);
            lv_led_set_color(tank_conn_led, COLOR_GREEN);
            lv_led_on(tank_conn_led);
            break;
        case TANK_DISCONNECTED:
            lv_label_set_text(tank_conn_label, "未连接");
            lv_obj_set_style_text_color(tank_conn_label, COLOR_RED, 0);
            lv_led_set_color(tank_conn_led, COLOR_RED);
            lv_led_on(tank_conn_led);
            break;
    }
}

void cooling_ui_set_valve_state(valve_state_t state)
{
    switch (state) {
        case VALVE_OPENED:
            lv_label_set_text(valve_label, "已打开");
            lv_obj_set_style_text_color(valve_label, COLOR_GREEN, 0);
            lv_led_set_color(valve_led, COLOR_GREEN);
            lv_led_on(valve_led);
            break;
        case VALVE_CLOSED:
            lv_label_set_text(valve_label, "关闭");
            lv_obj_set_style_text_color(valve_label, COLOR_TEXT_LIGHT, 0);
            lv_led_set_color(valve_led, lv_color_hex(0x757575));
            lv_led_on(valve_led);
            break;
        case VALVE_FAULT:
            lv_label_set_text(valve_label, "故障");
            lv_obj_set_style_text_color(valve_label, COLOR_RED, 0);
            lv_led_set_color(valve_led, COLOR_RED);
            lv_led_on(valve_led);
            break;
    }
}

void cooling_ui_set_heater_state(heater_state_t state)
{
    switch (state) {
        case HEATER_ON:
            lv_label_set_text(heater_label, "已开启");
            lv_obj_set_style_text_color(heater_label, COLOR_ORANGE, 0);
            lv_led_set_color(heater_led, COLOR_ORANGE);
            lv_led_on(heater_led);
            break;
        case HEATER_OFF:
            lv_label_set_text(heater_label, "未开启");
            lv_obj_set_style_text_color(heater_label, COLOR_TEXT_LIGHT, 0);
            lv_led_set_color(heater_led, lv_color_hex(0x757575));
            lv_led_on(heater_led);
            break;
				case HEATER_ERROR:
            lv_label_set_text(heater_label, "故障");
            lv_obj_set_style_text_color(heater_label, COLOR_RED, 0);
            lv_led_set_color(heater_led, COLOR_RED);
            lv_led_on(heater_led);
            break;
    }
}
