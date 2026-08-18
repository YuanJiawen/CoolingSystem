/**
 * @file    test_event_framework.c
 * @brief   事件框架 event_framework 的 host 单元测试(fake PRIMASK + 真 lwrb)
 *
 * 覆盖:
 *   1. init + publish_unique + dispatch:处理器收到正确 id/param
 *   2. 去重:消费前重复发布,一次投递;消费后可再发布
 *   3. dispatch 无事件/无处理器:空转返回 0,无处理器事件返回 1
 *   4. register_handler 参数校验(NULL 拒绝)
 *   5. 满载覆盖:有效容量 32 事件,发布 34 个不同 id → 最旧 2 个被丢,
 *      其余按序投递,被丢 id 的 pending 位已清(可再次发布)
 *   6. 临界区配平(fake PRIMASK):publish/dispatch 均在关中断区内,
 *      且每次调用后恢复开中断
 *
 * 构建/运行:sh run_event_framework.sh(clang wasm + node)
 */

#include "event_framework.h"

static int g_failures = 0;
int g_last_fail_line = 0;
#include "check.h"

/* ==================== 最小 libc 桩(optnone 防编译器递归展开) ==================== */

__attribute__((optnone)) void *memset(void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char *)s;
    while (n--) { *p++ = (unsigned char)c; }
    return s;
}

__attribute__((optnone)) void *memcpy(void *d, const void *s, size_t n)
{
    unsigned char *dp = (unsigned char *)d;
    const unsigned char *sp = (const unsigned char *)s;
    while (n--) { *dp++ = *sp++; }
    return d;
}

/* ==================== fake PRIMASK(临界区仪器化) ==================== */

static uint32_t g_primask      = 0;  /* 0=开中断,1=关中断(初始开) */
static int      g_disable_hits = 0;  /* __disable_irq 调用次数 */

uint32_t fake_get_primask(void) { return g_primask; }
void     fake_disable_irq(void) { g_primask = 1u; g_disable_hits++; }
void     fake_set_primask(uint32_t m) { g_primask = m; }
void     fake_enable_irq(void) { g_primask = 0u; }

/* ==================== 测试观察器 ==================== */

static uint8_t  g_last_evt_id = 0;
static uint16_t g_last_evt_param = 0;
static int      g_handler_calls = 0;

static uint8_t recording_handler(const Event_t *evt)
{
    g_last_evt_id = evt->event_id;
    g_last_evt_param = evt->param;
    g_handler_calls++;
    return 1;
}

/* ==================== 测试主体 ==================== */

int run_tests(void)
{
    g_failures = 0;

    /* ---- 场景 1:基本收发 ---- */
    CHECK(evt_framework_init() == 0, __LINE__);
    CHECK(evt_register_handler(0x80, recording_handler) == 0, __LINE__);
    g_primask = 0;
    g_disable_hits = 0;

    CHECK(evt_publish_unique(0x80, 42, NULL) == 0, __LINE__);
    CHECK(g_disable_hits > 0, __LINE__);   /* 发布发生在临界区内 */
    CHECK(g_primask == 0, __LINE__);       /* 退出后配平 */

    g_handler_calls = 0;
    g_last_evt_id = 0;
    evt_dispatch();
    CHECK(g_handler_calls == 1, __LINE__);
    CHECK(g_last_evt_id == 0x80, __LINE__);
    CHECK(g_last_evt_param == 42, __LINE__);
    CHECK(g_primask == 0, __LINE__);       /* dispatch 亦配平 */

    /* ---- 场景 2:去重与再发布 ---- */
    evt_publish_unique(0x80, 1, NULL);
    evt_publish_unique(0x80, 2, NULL);     /* 消费前重复 → 合并 */
    g_handler_calls = 0;
    CHECK(evt_dispatch() == 1, __LINE__);
    CHECK(evt_dispatch() == 0, __LINE__);  /* 第二次:队列空 */
    CHECK(g_handler_calls == 1, __LINE__);
    CHECK(g_last_evt_param == 1, __LINE__); /* 保留首次参数 */

    evt_publish_unique(0x80, 3, NULL);     /* 消费后可再发布 */
    g_handler_calls = 0;
    CHECK(evt_dispatch() == 1, __LINE__);
    CHECK(g_handler_calls == 1, __LINE__);
    CHECK(g_last_evt_param == 3, __LINE__);

    /* ---- 场景 3:无事件 / 无处理器 ---- */
    CHECK(evt_dispatch() == 0, __LINE__);  /* 队列空:静默返回 */
    evt_publish_unique(0x81, 7, NULL);     /* 0x81 未注册处理器 */
    CHECK(evt_dispatch() == 1, __LINE__);  /* 已出队即视为分发 */
    CHECK(g_primask == 0, __LINE__);

    /* ---- 场景 4:register 参数校验 ---- */
    CHECK(evt_register_handler(0x82, NULL) == -1, __LINE__);

    /* ---- 场景 5:满载覆盖(有效容量 = EVT_QUEUE_CAPACITY = 32 个事件;
     * lwrb 保留 1 字节,(CAPACITY+1) 缓冲尺寸恰好补偿) ---- */
    evt_framework_init();
    for (uint8_t id = 1; id <= 34; id++) {
        CHECK(evt_register_handler(id, recording_handler) == 0, __LINE__);
    }
    for (uint8_t id = 1; id <= 34; id++) {
        CHECK(evt_publish_unique(id, id, NULL) == 0, __LINE__);
    }
    g_handler_calls = 0;
    uint8_t first_id = 0, last_id = 0;
    for (int i = 0; i < 40; i++) {
        g_last_evt_id = 0;
        evt_dispatch();
        if (g_handler_calls == 1 && first_id == 0) { first_id = g_last_evt_id; }
        if (g_last_evt_id != 0) { last_id = g_last_evt_id; }
        g_handler_calls = 0;
    }
    CHECK(first_id == 3, __LINE__);   /* 溢出 2 个:最旧 id=1/2 已被覆盖丢弃 */
    CHECK(last_id == 34, __LINE__);   /* 最新的正常入队 */

    CHECK(evt_publish_unique(1, 9, NULL) == 0, __LINE__); /* 被丢 id 可再发布 */
    g_handler_calls = 0;
    g_last_evt_id = 0;
    evt_dispatch();
    CHECK(g_last_evt_id == 1, __LINE__);

    /* ---- 场景 6:未初始化防护 ---- */
    return g_failures;
}
