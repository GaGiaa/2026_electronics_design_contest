# STM32H723 双轮编码器轮式里程计测试

## 1. 功能说明

H723 使用 M2006 CAN ID `0x201` 和 `0x202` 的编码器反馈计算差速轮式里程计：

- ID 1：左轮，默认编码器方向 `+1`
- ID 2：右轮，默认编码器方向 `-1`
- 编码器：`8192 counts/电机转`
- M2006 减速比：`36:1`
- 轮径：`65 mm`
- 左右轮距：`205 mm`
- 底盘任务周期：`1 ms`

编码器方向会先转换为“车辆前进为正”的轮端位移。车体坐标为：`x` 向前、`y` 向左、`yaw` 逆时针为正。Watch 中的 `yaw_deg` 是归一化当前航向，范围为 `[-180, 180)`；`yaw_deg_continuous` 是不折返的累计航向，用于诊断实际转过了多少圈。

差速模型为：

```text
ds     = (dr + dl) / 2
dtheta = (dr - dl) / track_width
```

位置使用中间航向积分，轮端位移由累计电机 counts 换算。首次双轮有效反馈只建立基线，不产生位移。

## 2. Keil Watch 调试接口

Watch 可修改以下字段：

```text
g_h723_debug.wheel_odometry.wheel_diameter_mm
g_h723_debug.wheel_odometry.track_width_mm
g_h723_debug.wheel_odometry.left_encoder_sign
g_h723_debug.wheel_odometry.right_encoder_sign
g_h723_debug.wheel_odometry.reset_request
```

默认值为 `65.0`、`205.0`、`+1.0`、`-1.0`。方向参数只接受 `+1.0` 或 `-1.0`。修改轮径、轮距或方向后，应写入 `reset_request=1`，程序会在下一次有效双轮采样时重新建立零点，并自动把该请求恢复为 `0`。

主要只读字段：

```text
left_encoder / right_encoder
left_motor_counts / right_motor_counts
left_wheel_distance_mm / right_wheel_distance_mm
delta_left_mm / delta_right_mm / delta_distance_mm
delta_yaw_deg
x_mm / y_mm / yaw_deg / yaw_deg_continuous
linear_speed_mm_s / angular_speed_deg_s
left_feedback_age_ms / right_feedback_age_ms
valid / initialized / sample_count
reset_count / rebaseline_count
params_valid / params_rejected_count
```

`valid=0` 表示至少一路反馈超时或参数无效。反馈恢复后，系统先重新建立双轮 counts 基线，不会把断帧期间的 counts 差值突然积分进位姿。

## 3. VOFA 输出

将 `stm32h723_app/App/Inc/app_config.h` 中的 `APP_H723_WHEEL_ODOMETRY_VOFA_TELEMETRY_ENABLE` 改为 `1U`，重新编译后通过 UART8 发送 JustFloat。默认周期为 `10 ms`，该遥测与其他 UART8 遥测互斥。

15 个通道顺序固定为：

```text
1  left_wheel_distance_mm
2  right_wheel_distance_mm
3  delta_left_mm
4  delta_right_mm
5  delta_distance_mm
6  delta_yaw_deg
7  x_mm
8  y_mm
9  yaw_deg
10 linear_speed_mm_s
11 angular_speed_deg_s
12 left_feedback_age_ms
13 right_feedback_age_ms
14 valid
15 sample_count
```

## 4. 理论值

单个车轮输出轴转一圈时：

```text
轮周长 = pi * 65 = 204.2035 mm
输出轴一圈对应电机 counts = 8192 * 36 = 294912 counts
```

原地完整旋转一圈时，理想累计角为约 `+360` 或 `-360 deg`，但归一化的 `yaw_deg` 会回到初始角附近；例如从 `0 deg` 开始应显示约 `0 deg`，同时 `yaw_deg_continuous` 显示约 `360 deg` 或 `-360 deg`。若累计角也没有达到约一圈，先检查左右轮累计位移、编码器方向和反馈有效性；若累计角达到一圈但归一化角不回零，再检查轮距和轮胎打滑。

两轮等量同向转动时，理论上 `y=0`、`yaw=0`，`x` 等于左右轮平均位移。两轮等量反向转动时，理论上线位移接近 `0`，只有 `yaw` 变化。

## 5. 实车测试步骤

1. 首次测试将车架悬空，禁止电机输出。可以临时将 `APP_H723_CHASSIS_ACTUATION_ENABLE` 设为 `0U` 后重新编译，也可以保持 SE 未按下。
2. 确认 `g_h723_debug.fdcan.instance=2`、`m2006[0]` 收到 ID 1、`m2006[1]` 收到 ID 2，反馈年龄持续小于 `50 ms`。
3. 写入 `reset_request=1`，确认 `x/y/yaw` 清零且 `reset_request` 自动回到 `0`。
4. 手动向车辆前进方向转动左轮，确认 `left_wheel_distance_mm` 增加；转动右轮时确认经过默认 `-1` 符号后 `right_wheel_distance_mm` 也增加。
5. 两轮等量同向转动，检查 `x` 增加、`y` 和 `yaw` 基本不变。
6. 两轮反向等量转动，检查 `x` 基本不变，并确认 `yaw` 方向符合实际旋转方向。
7. 确认单轮输出轴一圈约为 `204.20 mm`。方向不对时只调整对应 `*_encoder_sign`，不要先修改轮径或轮距。
8. 低速闭环前，确认急停、CRSF 开关、CAN 反馈和电流限制均正常。当前代码未替代人工的电机和车架安全检查。

## 6. 主机测试

```powershell
& .\tests\test_stm32h723_wheel_odometry.ps1 -ProjectRoot $PWD
& .\tests\test_stm32h723_wheel_odometry_service.ps1 -ProjectRoot $PWD
& .\tests\test_stm32h723_wheel_odometry_static.ps1 -ProjectRoot $PWD
```

测试覆盖算法积分、编码器绕回、超时冻结、恢复重建基线、Watch 重置、非法参数、FDCAN2 ID 1/2 映射、Watch 字段、VOFA 通道和 Keil 工程源文件。
