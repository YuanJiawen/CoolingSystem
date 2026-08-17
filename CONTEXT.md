# CONTEXT.md — CoolingSystem 领域词汇

STM32F429 热玛吉美容仪皮肤冷却设备。本文档定义项目统一的领域语言,供架构评审、重构与代码走查使用。

## 核心概念

| 术语 | 含义 | 代码位置(现状) |
| --- | --- | --- |
| 冷却控制(Cooling Control) | 压力换算 + 阈值判定(罐连接/阀使能/超压)的纯计算模块;加热片为开环固定占空比(无 PID) | app/cooling_control.c |
| 压力采样(Pressure Sampling) | 双通道 ADC 串行采样 + 超压快路径 + ALARM GPIO 的硬件适配器(深模块:ISR 由模块接管,双缓冲消除混读竞态) | app/pressure_sampler.c |
| 压力通道(Pressure Channel) | 两个独立压力采样通道:ch1 = 冷却罐压力,ch2 = 管路压力 | `PressureSnapshot.pressure[0/1]`(app/pressure_sampler.c) |
| 超压告警(Overpressure Alarm) | 任一通道压力 > 130 PSI 时触发告警并关断加热片(两通道 PWM 归零,不动阀门);GPIO 快路径在采样模块 | `COOLING_CTRL_OVERPRESSURE_PSI` |
| 状态快照(Snapshot) | 采样器输出的只读压力快照(压力×2、valid、超压标志),由 app 层转发给 UI | `PressureSnapshot` |
| 事件框架(Event Framework) | ISR-safe 消息队列 + 按 event_id 分发(状态机接口已于 2026-08-10 删除) | event_framework/ |
| 冷却剂液位(Coolant Level) | 充足 / 尚可 / 耗尽,由高低两个液位传感器判定 | `on_valve_check_event` |

## 物理量约定

- 压力单位:PSI(传感器 0.5V + P×0.0266V,12 位 ADC,Vref=3V)
- 控制周期:500ms(TIM11 中断触发一轮双通道采样 + 加热/阀更新)
- PWM 输出范围:0 ~ 10000(TIM8 ARR=10000-1);加热片固定占空比见 `COOLING_CTRL_HEATER_DUTY_CH1/2`

## 架构原则(来自架构评审 2026-08-10)

- 控制逻辑为**纯计算模块**:输入压力值,输出阈值判定(罐连接/阀使能/超压);不直接触碰 HAL/引脚宏。
- 硬件访问(PWM/GPIO/ADC)位于控制模块 seam 之后的 **adapter**,由 app 层持有。
- UI 不依赖控制模块内部,只消费**状态快照**。
- 删除测试:无消费者的接口(如事件框架状态机 API、bsp_multibutton)应当删除而非保留。
