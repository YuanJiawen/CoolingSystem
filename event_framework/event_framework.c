/**
 * @file    event_framework.c
 * @brief   基于 lwrb 环形缓冲区的事件驱动框架实现
 */

#include "event_framework.h"
#include "lwrb.h"
#include <string.h>

/* ========================== 内部数据结构 ========================== */
static uint8_t evt_raw_buffer[(EVT_QUEUE_CAPACITY + 1) * sizeof(Event_t)];
static lwrb_t evt_ringbuf;

static EventHandler_fn evt_handlers[EVT_MAX + 1];
static StateHandler_fn state_handlers[SYS_STATE_MAX];
static EventDropHandler_fn global_drop_hook = NULL; 

static volatile SysState_e current_state;
static volatile uint8_t framework_initialized = 0;

/* ========================== 内部辅助函数 ========================== */

/**
 * @brief   覆盖写入，带有生命周期安全处理
 */
static lwrb_sz_t evt_ringbuf_write_overwrite(lwrb_t* buff, const void* data, lwrb_sz_t len) {
    lwrb_sz_t free_space = lwrb_get_free(buff);

    if (free_space < len) {
        lwrb_sz_t to_skip = len - free_space;
        lwrb_sz_t evt_size = (lwrb_sz_t)sizeof(Event_t);
        to_skip = ((to_skip + evt_size - 1) / evt_size) * evt_size; // 向上对齐到事件结构体大小

        // 在丢弃旧数据前，先窥探（Peek）旧事件，释放其 data_ptr
        if (global_drop_hook != NULL) {
            for (lwrb_sz_t offset = 0; offset < to_skip; offset += evt_size) {
                Event_t dropped_evt;
                if (lwrb_peek(buff, offset, &dropped_evt, evt_size) == evt_size) {
                    global_drop_hook(&dropped_evt); // 呼叫上层释放内存
                }
            }
        }

        lwrb_skip(buff, to_skip);
    }

    return lwrb_write(buff, data, len);
}

/* ========================== API 实现 ========================== */

int evt_framework_init(void) {
    if (lwrb_init(&evt_ringbuf, evt_raw_buffer, sizeof(evt_raw_buffer)) != 1) {
        return -1;
    }
    memset(evt_handlers, 0, sizeof(evt_handlers));
    memset(state_handlers, 0, sizeof(state_handlers));
    global_drop_hook = NULL;
    current_state = SYS_STATE_INIT;
    framework_initialized = 1;
    return 0;
}

void evt_register_drop_hook(EventDropHandler_fn drop_hook) {
    global_drop_hook = drop_hook;
}

int evt_publish(uint8_t event_id, uint16_t param, void* data_ptr) {
    Event_t evt = { .event_id = event_id, .param = param, .data_ptr = data_ptr };
    return evt_publish_event(&evt);
}

int evt_publish_event(const Event_t* evt) {
    if (!framework_initialized || evt == NULL) return -1;

    lwrb_sz_t written;
    EVT_ENTER_CRITICAL();
    written = evt_ringbuf_write_overwrite(&evt_ringbuf, (const uint8_t*)evt, (lwrb_sz_t)sizeof(Event_t));
    EVT_EXIT_CRITICAL();

    return (written == (lwrb_sz_t)sizeof(Event_t)) ? 0 : -1;
}

int evt_poll(Event_t* evt) {
    if (!framework_initialized || evt == NULL) return -1;

    lwrb_sz_t read_len;
    EVT_ENTER_CRITICAL();
    read_len = lwrb_read(&evt_ringbuf, (uint8_t*)evt, (lwrb_sz_t)sizeof(Event_t));
    EVT_EXIT_CRITICAL();

    return (read_len == (lwrb_sz_t)sizeof(Event_t)) ? 0 : -1;
}

int evt_register_handler(uint8_t event_id, EventHandler_fn handler) {
    if (event_id > EVT_MAX || handler == NULL) return -1;
    evt_handlers[event_id] = handler;
    return 0;
}

int evt_register_state_handler(SysState_e state, StateHandler_fn handler) {
    if (state >= SYS_STATE_MAX || handler == NULL) return -1;
    state_handlers[state] = handler;
    return 0;
}

void evt_set_state(SysState_e state) {
    if (state < SYS_STATE_MAX) current_state = state;
}

SysState_e evt_get_state(void) {
    return current_state;
}

void evt_dispatch(void) {
    Event_t evt;

    if (evt_poll(&evt) != 0) {
        return; // 无事件
    }

    uint8_t is_consumed = 0;

    // 1. 全局回调拦截层（如：底层错误处理、全局按键屏蔽等）
    if (evt.event_id <= EVT_MAX && evt_handlers[evt.event_id] != NULL) {
        is_consumed = evt_handlers[evt.event_id](&evt);
    }

    // 2. 如果未被全局拦截，送入状态机（增加 is_consumed 判断）
    if (!is_consumed) {
        SysState_e state = current_state;
        if (state < SYS_STATE_MAX && state_handlers[state] != NULL) {
            SysState_e new_state = state_handlers[state](&evt);
            if (new_state < SYS_STATE_MAX) {
                current_state = new_state;
            }
        }
    }
}

uint32_t evt_get_pending_count(void) {
    if (!framework_initialized) return 0;
    lwrb_sz_t bytes_full;
    EVT_ENTER_CRITICAL();
    bytes_full = lwrb_get_full(&evt_ringbuf);
    EVT_EXIT_CRITICAL();
    return (uint32_t)(bytes_full / sizeof(Event_t));
}

void evt_queue_flush(void) {
    if (!framework_initialized) return;
    EVT_ENTER_CRITICAL();
    // 队列清空时，如果有 Drop Hook 也应顺便清理未处理数据的内存，但为保持接口简单，这里仅 Reset
    // 如果工程中重度依赖内存池，此处也应添加 while(poll) + Drop hook 的逻辑
    lwrb_reset(&evt_ringbuf);
    EVT_EXIT_CRITICAL();
}