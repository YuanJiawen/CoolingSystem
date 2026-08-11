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
static EventDropHandler_fn global_drop_hook = NULL; 
static uint32_t evt_pending_bits[((EVT_MAX + 1U) + 31U) / 32U];

static volatile uint8_t framework_initialized = 0;

/* ========================== 内部辅助函数 ========================== */

/**
 * @brief   覆盖写入，带有生命周期安全处理
 */
static uint32_t evt_bit_mask(uint8_t event_id) {
    return 1UL << (event_id & 31U);
}

static uint32_t evt_bit_index(uint8_t event_id) {
    return (uint32_t)event_id >> 5U;
}

static uint8_t evt_is_pending(uint8_t event_id) {
    return (evt_pending_bits[evt_bit_index(event_id)] & evt_bit_mask(event_id)) != 0U;
}

static void evt_mark_pending(uint8_t event_id) {
    evt_pending_bits[evt_bit_index(event_id)] |= evt_bit_mask(event_id);
}

static void evt_clear_pending(uint8_t event_id) {
    evt_pending_bits[evt_bit_index(event_id)] &= ~evt_bit_mask(event_id);
}

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
                    evt_clear_pending(dropped_evt.event_id);
                    global_drop_hook(&dropped_evt); // 呼叫上层释放内存
                }
            }
        } else {
            for (lwrb_sz_t offset = 0; offset < to_skip; offset += evt_size) {
                Event_t dropped_evt;
                if (lwrb_peek(buff, offset, &dropped_evt, evt_size) == evt_size) {
                    evt_clear_pending(dropped_evt.event_id);
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
    memset(evt_pending_bits, 0, sizeof(evt_pending_bits));
    global_drop_hook = NULL;
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

int evt_publish_unique(uint8_t event_id, uint16_t param, void* data_ptr) {
    Event_t evt = { .event_id = event_id, .param = param, .data_ptr = data_ptr };
    return evt_publish_unique_event(&evt);
}

int evt_publish_unique_event(const Event_t* evt) {
    if (!framework_initialized || evt == NULL) return -1;

    lwrb_sz_t written = 0;
    EVT_ENTER_CRITICAL();
    if (!evt_is_pending(evt->event_id)) {
        written = evt_ringbuf_write_overwrite(&evt_ringbuf, (const uint8_t*)evt, (lwrb_sz_t)sizeof(Event_t));
        if (written == (lwrb_sz_t)sizeof(Event_t)) {
            evt_mark_pending(evt->event_id);
        }
    } else {
        written = (lwrb_sz_t)sizeof(Event_t);
    }
    EVT_EXIT_CRITICAL();

    return (written == (lwrb_sz_t)sizeof(Event_t)) ? 0 : -1;
}

int evt_poll(Event_t* evt) {
    if (!framework_initialized || evt == NULL) return -1;

    lwrb_sz_t read_len;
    EVT_ENTER_CRITICAL();
    read_len = lwrb_read(&evt_ringbuf, (uint8_t*)evt, (lwrb_sz_t)sizeof(Event_t));
    if (read_len == (lwrb_sz_t)sizeof(Event_t)) {
        evt_clear_pending(evt->event_id);
    }
    EVT_EXIT_CRITICAL();

    return (read_len == (lwrb_sz_t)sizeof(Event_t)) ? 0 : -1;
}

int evt_register_handler(uint8_t event_id, EventHandler_fn handler) {
    if (event_id > EVT_MAX || handler == NULL) return -1;
    evt_handlers[event_id] = handler;
    return 0;
}

void evt_dispatch(void) {
    Event_t evt;

    if (evt_poll(&evt) != 0) {
        return; // 无事件
    }

    if (evt.event_id <= EVT_MAX && evt_handlers[evt.event_id] != NULL) {
        (void)evt_handlers[evt.event_id](&evt);
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
    memset(evt_pending_bits, 0, sizeof(evt_pending_bits));
    EVT_EXIT_CRITICAL();
}
