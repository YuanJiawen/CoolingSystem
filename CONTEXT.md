# CONTEXT.md — CoolingSystem 领域词汇

STM32F429 热玛吉美容仪皮肤冷却设备。本文档定义项目统一的领域语言,供架构评审、重构与代码走查使用。

## 核心概念

| 术语 | 含义 | 代码位置(现状) |
| --- | --- | --- |
| 冷却控制(Cooling Control) | 采集压力 → 换算 → PID → 限幅 → PWM 输出的闭环回路,已深化为独立纯计算模块 | app/cooling_control.c |
| 压力通道(Pressure Channel) | 两个独立控制回路:ch1 = 冷却罐压力,ch2 = 管路压力 | `cooling_ctrls[0/1]`(app/app_control.c) |
| 设定值(Setpoint) | 目标压力(PSI),默认 50 PSI,可由上位机/UI 修改 | `target_pressure_ch1/2` |
| 超压告警(Overpressure Alarm) | 任一通道压力 > 130 PSI 时触发告警(安全功能,计划改中断快路径) | `COOLING_CTRL_OVERPRESSURE_PSI` |
| 状态快照(Control Snapshot) | 控制模块输出的只读状态集合(压力×2、控制量×2、告警、液位),由 app 层转发给 UI | `CoolingCtrlOut` |
| 事件框架(Event Framework) | ISR-safe 消息队列 + 按 event_id 分发(状态机接口已于 2026-08-10 删除) | event_framework/ |
| 冷却剂液位(Coolant Level) | 充足 / 尚可 / 耗尽,由高低两个液位传感器判定 | `on_valve_check_event` |

## 物理量约定

- 压力单位:PSI(传感器 0.5V + P×0.0266V,12 位 ADC,Vref=3V)
- 控制周期:500ms(TIM11 中断发布 ADC/PID 事件),采样时间 `pid->T = 0.5s`(计划单源化)
- PWM 输出范围:0 ~ 10000(TIM8 ARR=10000-1,计划单源化)

## 架构原则(来自架构评审 2026-08-10)

- 控制逻辑为**纯计算模块**:输入压力值+设定值,输出控制量;不直接触碰 HAL/引脚宏。
- 硬件访问(PWM/GPIO/ADC)位于控制模块 seam 之后的 **adapter**,由 app 层持有。
- UI 不依赖控制模块内部,只消费**状态快照**。
- 删除测试:无消费者的接口(如事件框架状态机 API、bsp_multibutton)应当删除而非保留。
