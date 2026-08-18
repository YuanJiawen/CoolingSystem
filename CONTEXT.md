# CONTEXT.md — CoolingSystem 领域词汇

STM32F429 热玛吉美容仪皮肤冷却设备。本文档定义项目统一的领域语言,供架构评审、重构与代码走查使用。

## 核心概念

| 术语 | 含义 | 代码位置(现状) |
| --- | --- | --- |
| 冷却控制(Cooling Control) | 压力换算 + 阈值判定(罐连接/阀使能/超压)的纯计算模块;含显示钳位纯函数;加热片为开环固定占空比(无 PID) | app/cooling_control.c |
| 冷却执行器(Cooling Actuator) | 安全输出深模块:加热片 PWM/电磁阀/超压告警的唯一写者;「超压→加热归零+告警置位」的映射住进模块(OverpressureLatch);上电先归零再启动 PWM 为模块不变量;工作模式与仿真模式共享同一接口 | app/cooling_actuator.c |
| 压力采样(Pressure Sampling) | 双通道 ADC 串行采样 + 超压快路径的硬件适配器(深模块:ISR 由模块接管,双缓冲消除混读竞态;快照发布/读取置于 PRIMASK 临界区,12 字节拷贝不可撕裂;超压动作委托执行器) | app/pressure_sampler.c |
| 压力通道(Pressure Channel) | 两个独立压力采样通道:ch1 = 冷却罐压力,ch2 = 管路压力 | `PressureSnapshot.pressure[0/1]`(app/pressure_sampler.c) |
| 超压告警(Overpressure Alarm) | 任一通道压力 > 130 PSI 时触发告警并关断加热片(两通道 PWM 归零,不动阀门);ISR 快路径经执行器锁存,不等主循环事件 | `COOLING_CTRL_OVERPRESSURE_PSI` |
| 状态快照(Snapshot) | 采样器输出的只读压力快照(压力×2、valid、超压标志),临界区内原子发布,由 app 层转发给 UI | `PressureSnapshot` |
| 显示钳位(Display Clamp) | 显示路径的压力钳位纯函数:<1 PSI 零点死区归零 + 钳 0~150 PSI,单源 `CoolingControl_DisplayPressure`;控制判定一律用原始压力(行为不变) | app/cooling_control.c |
| 事件框架(Event Framework) | ISR-safe 消息队列 + 按 event_id 分发;接口已于 2026-08-18 收缩为 init / publish_unique / register_handler / dispatch 四个(均有宿主测试,含满载覆盖与临界区配平);dispatch 返回 1/0 上报是否分发,主循环据此 __WFI 睡眠 | event_framework/ |
| 冷却剂液位(Coolant Level) | 充足 / 尚可 / 耗尽,由高低两个液位传感器判定 | `on_valve_check_event` |

## 物理量约定

- 压力单位:PSI(传感器 0.5V + P×0.0266V,12 位 ADC,Vref=3V)
- 定时器周期(唯一事实源 = .ioc 与 tim.c,已同步):TIM11 = 500ms 控制周期(采样+加热/阀更新);TIM13 = 10ms LVGL 时基;TIM14 = 1s 液位检查
- PWM 输出范围:0 ~ 10000(TIM8 ARR=10000-1);加热片固定占空比见 `COOLING_CTRL_HEATER_DUTY_CH1/2`,上电由冷却执行器保证零输出启动

## 架构原则(来自架构评审 2026-08-10 / 2026-08-18)

- 控制逻辑为**纯计算模块**:输入压力值,输出阈值判定(罐连接/阀使能/超压);不直接触碰 HAL/引脚宏。
- 硬件访问(PWM/GPIO/ADC)位于控制模块 seam 之后的 **adapter**,由 app 层持有;安全输出(TIM8 PWM / ALARM_EN / PRESSURE_CTRL_EN)进一步集中于**冷却执行器**单写者,仿真与工作模式共用其接口。
- UI 不依赖控制模块内部,只消费**状态快照**;显示路径的压力值经**显示钳位**单源函数。UI setter 内置**无变化守卫**(压力 0.1 PSI 粒度 / 离散状态 / 超警 show-hide):显示值未变则不触碰任何 LVGL 对象;仪表指针依赖 lv_meter 内置的新旧角度 bbox 精准失效,不做全仪表重绘(若上板复现指针拖影,回退为值变化时全仪表 invalidate)。
- 删除测试:无消费者的接口(如事件框架状态机 API、bsp_multibutton)应当删除而非保留。
- 宿主测试(tests/host,clang→wasm→node)是各模块的测试面:共享装备在 `tests/host/fake/`(check.h、桩 HAL)与 `run_common.sh`;新增套件只需一个 .c 测试文件 + 三行 runner。
