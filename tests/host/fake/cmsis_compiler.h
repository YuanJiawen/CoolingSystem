/**
 * @file    fake/cmsis_compiler.h
 * @brief   host 测试用 CMSIS 临界区桩(PRIMASK 仪器化,由各测试替身提供实现)
 *
 * 与目标侧 CMSIS 同名同语义;fake 记录关中断事件,供测试断言
 * 「共享状态访问发生在临界区内」与「保存/恢复配平」。
 */
#ifndef FAKE_CMSIS_COMPILER_H
#define FAKE_CMSIS_COMPILER_H

#include <stdint.h>

uint32_t fake_get_primask(void);
void     fake_disable_irq(void);
void     fake_set_primask(uint32_t primask);
void     fake_enable_irq(void);

#define __get_PRIMASK()  fake_get_primask()
#define __disable_irq()  fake_disable_irq()
#define __set_PRIMASK(m) fake_set_primask((m))
#define __enable_irq()   fake_enable_irq()

#endif /* FAKE_CMSIS_COMPILER_H */
