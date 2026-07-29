#ifndef MOTOR_PID_APP_MATH_H
#define MOTOR_PID_APP_MATH_H

static inline float App_Math_ClampFloat(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

#endif
