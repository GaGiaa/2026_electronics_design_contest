# PID source origin

This directory is a controlled copy of the platform-independent PID core from
`D:\desktop\2026RC\Control\single_motor_test\Libraries\MotorLib`.

Only the PID implementation and its clamp helper are used by this MSPM0 PWM
motor application. CAN protocols, STM32 HAL adapters, DJI code, and RobStride
code are deliberately not copied.
