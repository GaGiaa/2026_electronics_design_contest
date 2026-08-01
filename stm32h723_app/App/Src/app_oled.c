#include "app_oled.h"
#include "app_pipe_startup.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "app_chassis.h"
#include "app_debug.h"
#include "app_task2.h"
#include "app_task4.h"
#include "app_task56.h"
#include "i2c.h"
#include "oled_font.h"

#define BOARD_OLED_WIDTH                  128U
#define BOARD_OLED_PAGE_COUNT              8U
#define BOARD_OLED_FRAMEBUFFER_SIZE      (BOARD_OLED_WIDTH * BOARD_OLED_PAGE_COUNT)
#define BOARD_OLED_I2C_CONTROL_COMMAND  0x00U
#define BOARD_OLED_I2C_CONTROL_DATA     0x40U

static uint8_t s_framebuffer[BOARD_OLED_FRAMEBUFFER_SIZE];
static uint8_t s_tx_buffer[BOARD_OLED_WIDTH + 1U];
static uint8_t s_column;
static uint8_t s_page;
static uint32_t s_last_attempt_ms;
static uint32_t s_last_update_ms;
static uint32_t s_display_count;
static bool s_service_initialized;
static HAL_StatusTypeDef s_last_hal_status = HAL_OK;

static void recover_i2c4(void)
{
    g_h723_debug.oled.i2c_recovery_status = HAL_I2C_DeInit(&hi2c4);
    g_h723_debug.oled.i2c_recovery_count++;
    MX_I2C4_Init();
}

static board_oled_status_t write_i2c(const uint8_t *data, uint16_t length)
{
    if ((data == NULL) || (length == 0U) ||
        (length > (BOARD_OLED_WIDTH + 1U))) {
        s_last_hal_status = HAL_ERROR;
        return BOARD_OLED_STATUS_ARGUMENT;
    }

    s_last_hal_status = HAL_I2C_Master_Transmit(
        &hi2c4,
        (uint16_t)(APP_H723_OLED_I2C_ADDRESS << 1U),
        (uint8_t *)data,
        length,
        APP_H723_OLED_I2C_TIMEOUT_MS);
    if (s_last_hal_status == HAL_OK) {
        return BOARD_OLED_STATUS_OK;
    }
    if (s_last_hal_status == HAL_TIMEOUT) {
        return BOARD_OLED_STATUS_TIMEOUT;
    }
    return BOARD_OLED_STATUS_BUS_ERROR;
}

static board_oled_status_t write_command(const uint8_t *commands,
                                         uint8_t length)
{
    if ((commands == NULL) || (length == 0U) || (length > BOARD_OLED_WIDTH)) {
        return BOARD_OLED_STATUS_ARGUMENT;
    }

    s_tx_buffer[0] = BOARD_OLED_I2C_CONTROL_COMMAND;
    memcpy(&s_tx_buffer[1], commands, length);
    return write_i2c(s_tx_buffer, (uint16_t)length + 1U);
}

static board_oled_status_t write_data_page(uint8_t page)
{
    if (page >= BOARD_OLED_PAGE_COUNT) {
        return BOARD_OLED_STATUS_ARGUMENT;
    }

    s_tx_buffer[0] = BOARD_OLED_I2C_CONTROL_DATA;
    memcpy(&s_tx_buffer[1],
           &s_framebuffer[page * BOARD_OLED_WIDTH],
           BOARD_OLED_WIDTH);
    return write_i2c(s_tx_buffer, BOARD_OLED_WIDTH + 1U);
}

board_oled_status_t board_oled_init(void)
{
    static const uint8_t init_commands[] = {
        0xAEU,       /* display off */
        0xD5U, 0x80U, /* display clock divide */
        0xA8U, 0x3FU, /* multiplex ratio */
        0xD3U, 0x00U, /* display offset */
        0x40U,       /* start line */
        0x8DU, 0x14U, /* charge pump */
        0x20U, 0x02U, /* page addressing mode */
        0xA1U,       /* segment remap */
        0xC8U,       /* COM scan direction */
        0xDAU, 0x12U, /* COM pins */
        0x81U, 0x7FU, /* contrast */
        0xD9U, 0xF1U, /* pre-charge */
        0xDBU, 0x40U, /* VCOMH */
        0xA4U,       /* resume RAM display */
        0xA6U,       /* normal display */
        0xAFU        /* display on */
    };
    board_oled_status_t status;

    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    s_column = 0U;
    s_page = 0U;
    status = write_command(init_commands, sizeof(init_commands));
    if (status != BOARD_OLED_STATUS_OK) {
        return status;
    }
    return board_oled_update();
}

board_oled_status_t board_oled_clear(void)
{
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    s_column = 0U;
    s_page = 0U;
    return BOARD_OLED_STATUS_OK;
}

board_oled_status_t board_oled_update(void)
{
    uint8_t page;
    uint8_t commands[3];
    board_oled_status_t status;

    for (page = 0U; page < BOARD_OLED_PAGE_COUNT; ++page) {
        commands[0] = (uint8_t)(0xB0U + page);
        commands[1] = 0x00U;
        commands[2] = 0x10U;
        status = write_command(commands, sizeof(commands));
        if (status != BOARD_OLED_STATUS_OK) {
            return status;
        }
        status = write_data_page(page);
        if (status != BOARD_OLED_STATUS_OK) {
            return status;
        }
    }
    return BOARD_OLED_STATUS_OK;
}

board_oled_status_t board_oled_set_cursor(uint8_t column, uint8_t page)
{
    if ((column >= BOARD_OLED_WIDTH) || (page >= BOARD_OLED_PAGE_COUNT)) {
        return BOARD_OLED_STATUS_ARGUMENT;
    }
    s_column = column;
    s_page = page;
    return BOARD_OLED_STATUS_OK;
}

board_oled_status_t board_oled_write_char(char value)
{
    uint8_t index;

    if ((value < (char)BOARD_OLED_FONT_FIRST_ASCII) ||
        (value > (char)BOARD_OLED_FONT_LAST_ASCII)) {
        value = '?';
    }
    if ((s_page >= BOARD_OLED_PAGE_COUNT) ||
        (s_column + BOARD_OLED_FONT_WIDTH >= BOARD_OLED_WIDTH)) {
        return BOARD_OLED_STATUS_ARGUMENT;
    }

    index = (uint8_t)value - BOARD_OLED_FONT_FIRST_ASCII;
    memcpy(&s_framebuffer[s_page * BOARD_OLED_WIDTH + s_column],
           g_board_oled_font[index], BOARD_OLED_FONT_WIDTH);
    s_column += BOARD_OLED_FONT_WIDTH + 1U;
    return BOARD_OLED_STATUS_OK;
}

board_oled_status_t board_oled_write_string(const char *text)
{
    board_oled_status_t status;

    if (text == NULL) {
        return BOARD_OLED_STATUS_ARGUMENT;
    }
    while (*text != '\0') {
        if (*text == '\n') {
            s_column = 0U;
            if (s_page + 1U >= BOARD_OLED_PAGE_COUNT) {
                return BOARD_OLED_STATUS_ARGUMENT;
            }
            ++s_page;
            ++text;
            continue;
        }
        if (s_column + BOARD_OLED_FONT_WIDTH >= BOARD_OLED_WIDTH) {
            s_column = 0U;
            if (s_page + 1U >= BOARD_OLED_PAGE_COUNT) {
                return BOARD_OLED_STATUS_ARGUMENT;
            }
            ++s_page;
        }
        status = board_oled_write_char(*text++);
        if (status != BOARD_OLED_STATUS_OK) {
            return status;
        }
    }
    return BOARD_OLED_STATUS_OK;
}

static const char *oled_mode_name(uint32_t mode)
{
    switch (mode) {
    case APP_CHASSIS_MODE_TASK_MENU:
        return "MENU";
    case APP_CHASSIS_MODE_REMOTE_MANUAL:
        return "MANUAL";
    case APP_CHASSIS_MODE_REMOTE_LINE_FOLLOW:
        return "LINE FOLLOW";
    case APP_CHASSIS_MODE_REMOTE_LINE_FOLLOW_BALL:
        return "BALL FOLLOW";
    case APP_CHASSIS_MODE_REMOTE_IDLE:
    default:
        return "IDLE";
    }
}

static const char *oled_task2_phase_name(uint32_t phase)
{
    static const char *const names[] = {
        "IDLE", "RUNNING", "STOPPED"
    };
    return (phase < (sizeof(names) / sizeof(names[0]))) ? names[phase] : "UNKNOWN";
}

static bool oled_should_show_task2(void)
{
    if (g_h723_debug.task2.phase == APP_TASK2_PHASE_RUNNING) {
        return true;
    }
    return (g_h723_debug.task2.phase == APP_TASK2_PHASE_STOPPED) &&
           (g_h723_debug.control.selected_task == APP_TASK_MENU_FIRST_TASK_ID);
}

static bool oled_should_show_task4(void)
{
    if (g_h723_debug.task4.phase == APP_TASK4_PHASE_RUNNING) {
        return true;
    }
    return (g_h723_debug.task4.phase == APP_TASK4_PHASE_STOPPED) &&
           (g_h723_debug.control.selected_task == 4U);
}

static bool oled_should_show_task56(void)
{
    const bool selected_task_matches =
        g_h723_debug.task56.task_id == g_h723_debug.control.selected_task;

    if (selected_task_matches &&
        (g_h723_debug.task56.phase == APP_TASK56_PHASE_RUNNING)) {
        return true;
    }
    return selected_task_matches &&
           (g_h723_debug.task56.phase == APP_TASK56_PHASE_STOPPED);
}

static board_oled_status_t render_runtime_page(void)
{
    char line[24];
    const bool show_task2 = oled_should_show_task2();
    const bool show_task4 = oled_should_show_task4();
    const bool show_task56 = oled_should_show_task56();
    const bool show_task_page = show_task2 || show_task4 || show_task56;
    const uint32_t task_phase = show_task56 ? g_h723_debug.task56.phase :
                                (show_task4 ? g_h723_debug.task4.phase :
                                 g_h723_debug.task2.phase);
    const uint32_t task_elapsed_ms = show_task56 ?
        g_h723_debug.task56.elapsed_ms : (show_task4 ?
        g_h723_debug.task4.elapsed_ms : g_h723_debug.task2.elapsed_ms);
    const float task_speed_mm_s = show_task56 ?
        g_h723_debug.task56.base_speed_mm_s : (show_task4 ?
        g_h723_debug.task4.base_speed_mm_s : g_h723_debug.task2.base_speed_mm_s);
    const char *title;
    board_oled_status_t status;

    status = board_oled_clear();
    if (status != BOARD_OLED_STATUS_OK) {
        return status;
    }
    status = board_oled_set_cursor(0U, 0U);
    if (status != BOARD_OLED_STATUS_OK) {
        return status;
    }
    if (g_h723_debug.pipe_startup.state ==
        (uint32_t)APP_PIPE_STARTUP_STATE_CALIBRATION_REQUIRED) {
        status = board_oled_write_string("CALIBRATE PIPE");
        if (status != BOARD_OLED_STATUS_OK) { return status; }
        status = board_oled_set_cursor(0U, 2U);
        if (status != BOARD_OLED_STATUS_OK) { return status; }
        if (g_h723_debug.jy901s.sample_valid != 0U &&
            g_h723_debug.jy901s.calibration_valid != 0U &&
            g_h723_debug.jy901s.sample_age_ms <= APP_H723_TILT_CONTROL_SAMPLE_MAX_AGE_MS) {
            (void)snprintf(line, sizeof(line), "PITCH:%.2f",
                           (double)g_h723_debug.jy901s.vehicle_angle_deg[1]);
        } else {
            (void)snprintf(line, sizeof(line), "IMU:WAIT");
        }
        status = board_oled_write_string(line);
        if (status != BOARD_OLED_STATUS_OK) { return status; }
        status = board_oled_set_cursor(0U, 4U);
        if (status != BOARD_OLED_STATUS_OK) { return status; }
        status = board_oled_write_string("B1:CAPTURE");
        if (status != BOARD_OLED_STATUS_OK) { return status; }
        status = board_oled_set_cursor(0U, 6U);
        if (status != BOARD_OLED_STATUS_OK) { return status; }
        status = board_oled_write_string("B2:SKIP");
        if (status != BOARD_OLED_STATUS_OK) { return status; }
        return board_oled_update();
    }
    if (g_h723_debug.control.remote_takeover) {
        title = "REMOTE CONTROL";
    } else if (g_h723_debug.control.selected_task == 3U) {
        title = "TASK 3 N/A";
    } else if (show_task56) {
        title = g_h723_debug.task56.task_id == 6U ? "TASK 6" : "TASK 5";
    } else if (show_task4) {
        title = "TASK 4";
    } else {
        title = show_task2 ? "TASK 2" : "TASK MENU";
    }
    status = board_oled_write_string(title);
    if (status != BOARD_OLED_STATUS_OK) {
        return status;
    }
    status = board_oled_set_cursor(0U, 2U);
    if (status != BOARD_OLED_STATUS_OK) {
        return status;
    }
    if (g_h723_debug.control.remote_takeover) {
        (void)snprintf(line, sizeof(line), "MODE:%s",
                       oled_mode_name(g_h723_debug.control.mode));
    } else if (g_h723_debug.control.selected_task == 3U) {
        (void)snprintf(line, sizeof(line), "NOT IMPLEMENTED");
    } else if (show_task_page) {
        (void)snprintf(line, sizeof(line), "STATE:%s",
                       oled_task2_phase_name(task_phase));
    } else {
        (void)snprintf(line, sizeof(line), "TASK:%lu",
                       (unsigned long)g_h723_debug.control.selected_task);
    }
    status = board_oled_write_string(line);
    if (status != BOARD_OLED_STATUS_OK) {
        return status;
    }
    status = board_oled_set_cursor(0U, 4U);
    if (status != BOARD_OLED_STATUS_OK) {
        return status;
    }
    if (g_h723_debug.control.remote_takeover) {
        (void)snprintf(line, sizeof(line), "SB:%lu SC:%lu",
                       (unsigned long)g_h723_debug.control.sb_state,
                       (unsigned long)g_h723_debug.control.sc_state);
    } else if (show_task_page) {
        (void)snprintf(line, sizeof(line), "SPEED:%lumm/s",
                       (unsigned long)task_speed_mm_s);
        status = board_oled_write_string(line);
        if (status != BOARD_OLED_STATUS_OK) { return status; }
        status = board_oled_set_cursor(0U, 6U);
        if (status != BOARD_OLED_STATUS_OK) { return status; }
        (void)snprintf(line, sizeof(line), "TIME:%lu.%03lus",
                       (unsigned long)(task_elapsed_ms / 1000U),
                       (unsigned long)(task_elapsed_ms % 1000U));
        status = board_oled_write_string(line);
        if (status != BOARD_OLED_STATUS_OK) {
            return status;
        }
        return board_oled_update();
    } else {
        status = board_oled_write_string("B1:OK B2:UP");
        if (status != BOARD_OLED_STATUS_OK) {
            return status;
        }
        status = board_oled_set_cursor(0U, 6U);
        if (status != BOARD_OLED_STATUS_OK) {
            return status;
        }
        status = board_oled_write_string("B3:DOWN");
        if (status != BOARD_OLED_STATUS_OK) {
            return status;
        }
        return board_oled_update();
    }
    status = board_oled_write_string(line);
    if (status != BOARD_OLED_STATUS_OK) {
        return status;
    }
    status = board_oled_set_cursor(0U, 6U);
    if (status != BOARD_OLED_STATUS_OK) {
        return status;
    }
    (void)snprintf(line, sizeof(line), "LINK AGE:%lums",
                   (unsigned long)g_h723_debug.crsf.age_ms);
    status = board_oled_write_string(line);
    if (status != BOARD_OLED_STATUS_OK) {
        return status;
    }
    return board_oled_update();
}

static void record_oled_status(board_oled_status_t status)
{
    g_h723_debug.oled.last_hal_status = (uint32_t)s_last_hal_status;
    g_h723_debug.oled.i2c_error_code = hi2c4.ErrorCode;
    g_h723_debug.oled.i2c_state = (uint32_t)hi2c4.State;
    g_h723_debug.oled.i2c_isr = I2C4->ISR;
    g_h723_debug.oled.gpio_pd_idr = GPIOD->IDR;
    if (status == BOARD_OLED_STATUS_OK) {
        g_h723_debug.oled.initialized = 1U;
    } else {
        g_h723_debug.oled.initialized = 0U;
        g_h723_debug.oled.error_count++;
        s_service_initialized = false;
        if (status != BOARD_OLED_STATUS_ARGUMENT) {
            /* HAL timeout can leave I2C4 BUSY with SCL held by the master. */
            recover_i2c4();
        }
    }
}

void h723_oled_service_init(void)
{
    g_h723_debug.oled.enabled = 1U;
    g_h723_debug.oled.initialized = 0U;
    g_h723_debug.oled.init_attempt_count = 0U;
    g_h723_debug.oled.last_hal_status = HAL_OK;
    g_h723_debug.oled.i2c_error_code = HAL_I2C_ERROR_NONE;
    g_h723_debug.oled.i2c_state = (uint32_t)hi2c4.State;
    g_h723_debug.oled.i2c_isr = I2C4->ISR;
    g_h723_debug.oled.gpio_pd_idr = GPIOD->IDR;
    g_h723_debug.oled.i2c_recovery_count = 0U;
    g_h723_debug.oled.i2c_recovery_status = HAL_OK;
    g_h723_debug.oled.update_count = 0U;
    g_h723_debug.oled.error_count = 0U;
    s_last_attempt_ms = 0U - APP_H723_OLED_REFRESH_PERIOD_MS;
    s_last_update_ms = 0U;
    s_display_count = 0U;
    s_service_initialized = false;
}

void h723_oled_service_step(uint32_t now_ms)
{
    board_oled_status_t status;

    if (!s_service_initialized) {
        if ((uint32_t)(now_ms - s_last_attempt_ms) <
            APP_H723_OLED_REFRESH_PERIOD_MS) {
            return;
        }
        s_last_attempt_ms = now_ms;
        g_h723_debug.oled.init_attempt_count++;
        status = board_oled_init();
        if (status != BOARD_OLED_STATUS_OK) {
            record_oled_status(status);
            return;
        }
        status = render_runtime_page();
        record_oled_status(status);
        if (status == BOARD_OLED_STATUS_OK) {
            s_service_initialized = true;
            s_last_update_ms = now_ms;
            g_h723_debug.oled.update_count++;
            s_display_count++;
        }
        return;
    }

    if ((uint32_t)(now_ms - s_last_update_ms) <
        APP_H723_OLED_REFRESH_PERIOD_MS) {
        return;
    }
    s_last_update_ms = now_ms;
    status = render_runtime_page();
    record_oled_status(status);
    if (status == BOARD_OLED_STATUS_OK) {
        g_h723_debug.oled.update_count++;
        s_display_count++;
    }
}
