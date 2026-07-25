# PID 源码来源

本目录是从外部 MotorLib PID 实现中整理出的、与平台无关的 PID 核心代码副本。

本 MSPM0 PWM 电机应用只使用 PID 实现及其限幅辅助函数。
CAN 协议、STM32 HAL 适配层、DJI 代码和 RobStride 代码均未复制到本工程。
