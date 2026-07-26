#ifndef CRSF_CONFIG_H
#define CRSF_CONFIG_H

#include "protocols/crsf/crsf_protocol.h"

/* CRSF 遥控控制功能开关：1 表示启用，0 表示禁用。 */
#ifndef CRSF_REMOTE_CONTROL_ENABLE
#define CRSF_REMOTE_CONTROL_ENABLE 1U
#endif

/* 摇杆满量程对应的车轮目标速度，单位为 mm/s。 */
#ifndef CRSF_MAX_SPEED_MM_PER_S
#define CRSF_MAX_SPEED_MM_PER_S 800.0f
#endif

/* 超过该时间未收到有效 CRSF 帧后停车，单位为 ms。 */
#ifndef CRSF_LINK_TIMEOUT_MS
#define CRSF_LINK_TIMEOUT_MS 100U
#endif

/* 前进/后退通道：当前遥控器左摇杆上下为 CH3，数组索引为 2。 */
#ifndef CRSF_FORWARD_CHANNEL_INDEX
#define CRSF_FORWARD_CHANNEL_INDEX 2U
#endif

/* 转向通道：当前遥控器右摇杆左右为 CH1，数组索引为 0。 */
#ifndef CRSF_TURN_CHANNEL_INDEX
#define CRSF_TURN_CHANNEL_INDEX 0U
#endif

/* 当前发射机 SB 三档开关使用 CH5，接收机通道数组索引为 6。 */
#ifndef CRSF_MODE_CHANNEL_INDEX
#define CRSF_MODE_CHANNEL_INDEX 6U
#endif

/* 当前发射机 SC 三档开关使用接收机通道数组索引 7。 */
#ifndef CRSF_SC_CHANNEL_INDEX
#define CRSF_SC_CHANNEL_INDEX 7U
#endif

/* SB 原始值低于/高于这两个阈值时分别进入低档/高档。 */
#ifndef CRSF_MODE_LOW_MAX
#define CRSF_MODE_LOW_MAX 700U
#endif
#ifndef CRSF_MODE_HIGH_MIN
#define CRSF_MODE_HIGH_MIN 1300U
#endif

/* 前进方向符号，设为 -1.0f 可反转前进/后退方向。 */
#ifndef CRSF_FORWARD_SIGN
#define CRSF_FORWARD_SIGN 1.0f
#endif

/* 转向方向符号，设为 -1.0f 可反转左转/右转方向。 */
#ifndef CRSF_TURN_SIGN
#define CRSF_TURN_SIGN 1.0f
#endif

/* CRSF 11-bit 通道的标准最小值。 */
#define CRSF_CHANNEL_MIN 172U
/* CRSF 通道中位值。 */
#define CRSF_CHANNEL_CENTER 992U
/* CRSF 11-bit 通道的标准最大值。 */
#define CRSF_CHANNEL_MAX 1811U
/* 摇杆中位死区，占归一化量程的比例。 */
#define CRSF_CHANNEL_DEADBAND 0.2f

#if (CRSF_FORWARD_CHANNEL_INDEX >= CRSF_CHANNEL_COUNT) || \
    (CRSF_TURN_CHANNEL_INDEX >= CRSF_CHANNEL_COUNT) || \
    (CRSF_MODE_CHANNEL_INDEX >= CRSF_CHANNEL_COUNT) || \
    (CRSF_SC_CHANNEL_INDEX >= CRSF_CHANNEL_COUNT)
#error "CRSF channel index must fit inside CRSF_CHANNEL_COUNT"
#endif
#if (CRSF_CHANNEL_MIN >= CRSF_CHANNEL_CENTER) || \
    (CRSF_CHANNEL_CENTER >= CRSF_CHANNEL_MAX)
#error "CRSF channel range must be strictly increasing"
#endif
#if (CRSF_LINK_TIMEOUT_MS == 0U)
#error "CRSF_LINK_TIMEOUT_MS must be nonzero"
#endif
#if (CRSF_MODE_LOW_MAX <= CRSF_CHANNEL_MIN) || \
    (CRSF_MODE_LOW_MAX >= CRSF_CHANNEL_CENTER) || \
    (CRSF_MODE_HIGH_MIN <= CRSF_CHANNEL_CENTER) || \
    (CRSF_MODE_HIGH_MIN >= CRSF_CHANNEL_MAX)
#error "CRSF mode thresholds must surround the channel center"
#endif

#endif
