#include "app_task4.h"

#include <stddef.h>
#include <string.h>

#include "app_config.h"

static bool app_task4_is_running(app_task4_phase_t phase)
{
    return phase == APP_TASK4_PHASE_RUNNING;
}

/**
 * @brief 根据已用时间和已驶距离计算目标速度。
 *
 * 速度曲线为梯形混合策略：
 * - 加速段（时间-based）：speed = accel * elapsed_s，上限为巡航速度。
 *   确保车辆从零开始有足够的初始加速度，不受编码器起步距离制约。
 * - 匀速段：speed = cruise_speed。
 * - 制动段（距离-based）：剩余距离 ≤ 制动距离时，speed 与剩余距离成线性关系，
 *   自动适应实际行驶路径长度。
 *
 * @param[in] traveled_mm 已行驶距离，单位 mm。
 * @param[in] elapsed_ms  已用时间，单位 ms。
 * @return 目标速度，单位 mm/s，范围 [0, APP_H723_TASK4_CRUISE_SPEED_MM_S]。
 */
static float app_task4_speed(float traveled_mm, uint32_t elapsed_ms)
{
    const float elapsed_s = (float)elapsed_ms / 1000.0f;
    const float remaining_mm = APP_H723_TASK4_A_TO_B_DISTANCE_MM - traveled_mm;
    float speed_mm_s;

    if (remaining_mm <= 0.0f) {
        return 0.0f;
    }

    /* 时间-based 加速上限 */
    speed_mm_s = APP_H723_TASK4_ACCEL_MM_S2 * elapsed_s;
    if (speed_mm_s > APP_H723_TASK4_CRUISE_SPEED_MM_S) {
        speed_mm_s = APP_H723_TASK4_CRUISE_SPEED_MM_S;
    }

    /* 距离-based 制动下限：接近 B 点时覆盖时间速度 */
    if (remaining_mm < APP_H723_TASK4_BRAKE_DISTANCE_MM) {
        const float brake_speed = APP_H723_TASK4_CRUISE_SPEED_MM_S *
                                  remaining_mm / APP_H723_TASK4_BRAKE_DISTANCE_MM;
        if (brake_speed < speed_mm_s) {
            speed_mm_s = brake_speed;
        }
    }

    if (speed_mm_s < 0.0f) {
        return 0.0f;
    }
    if (speed_mm_s > APP_H723_TASK4_CRUISE_SPEED_MM_S) {
        return APP_H723_TASK4_CRUISE_SPEED_MM_S;
    }
    return speed_mm_s;
}

void app_task4_init(app_task4_state_t *state)
{
    if (state != NULL) {
        (void)memset(state, 0, sizeof(*state));
    }
}

void app_task4_start(app_task4_state_t *state, uint32_t now_ms, float distance_mm)
{
    if (state == NULL) {
        return;
    }

    (void)memset(state, 0, sizeof(*state));
    state->phase = APP_TASK4_PHASE_RUNNING;
    state->start_ms = now_ms;
    state->last_step_ms = now_ms;
    state->start_distance_mm = distance_mm;
}

void app_task4_abort(app_task4_state_t *state)
{
    app_task4_init(state);
}

void app_task4_step(app_task4_state_t *state, const app_task4_input_t *input)
{
    uint32_t elapsed_ms;

    if ((state == NULL) || (input == NULL) || !app_task4_is_running(state->phase)) {
        return;
    }

    state->last_step_ms = input->now_ms;
    state->traveled_distance_mm = input->distance_mm - state->start_distance_mm;
    if (state->traveled_distance_mm < 0.0f) {
        state->traveled_distance_mm = 0.0f;
    }

    elapsed_ms = input->now_ms - state->start_ms;
    if (state->traveled_distance_mm >= APP_H723_TASK4_A_TO_B_DISTANCE_MM) {
        /* 已到达 B 点，距离优先触发停车。 */
        state->phase = APP_TASK4_PHASE_STOPPED;
        state->finish_ms = input->now_ms;
    } else if (elapsed_ms >= APP_H723_TASK4_RUN_TIMEOUT_MS) {
        /* 超时保护：超过 8 秒仍未到达目标距离时强制停车。 */
        state->phase = APP_TASK4_PHASE_STOPPED;
        state->finish_ms = state->start_ms + APP_H723_TASK4_RUN_TIMEOUT_MS;
    }
}

void app_task4_get_output(const app_task4_state_t *state, app_task4_output_t *output)
{
    uint32_t end_ms;
    uint32_t elapsed_ms;
    float traveled_mm;

    if ((state == NULL) || (output == NULL)) {
        return;
    }

    (void)memset(output, 0, sizeof(*output));
    output->phase = state->phase;
    output->running = app_task4_is_running(state->phase);
    output->follow_line = output->running;
    output->stop = state->phase == APP_TASK4_PHASE_STOPPED;
    end_ms = output->running ? state->last_step_ms : state->finish_ms;
    elapsed_ms = state->phase == APP_TASK4_PHASE_IDLE ?
                 0U : (end_ms - state->start_ms);
    output->elapsed_ms = elapsed_ms;
    traveled_mm = state->phase == APP_TASK4_PHASE_IDLE ?
                  0.0f : state->traveled_distance_mm;
    output->distance_mm = traveled_mm;
    output->base_speed_mm_s = output->running ?
        app_task4_speed(traveled_mm, elapsed_ms) : 0.0f;
}
