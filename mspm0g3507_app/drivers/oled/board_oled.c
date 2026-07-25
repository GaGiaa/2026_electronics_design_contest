#include "board_oled.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "config/app_config.h"
#include "drivers/oled/board_oled_font.h"
#include "ti_msp_dl_config.h"

#define BOARD_OLED_WIDTH                 128U
#define BOARD_OLED_PAGE_COUNT              8U
#define BOARD_OLED_FRAMEBUFFER_SIZE      (BOARD_OLED_WIDTH * BOARD_OLED_PAGE_COUNT)
#define BOARD_OLED_I2C_CONTROL_COMMAND  0x00U
#define BOARD_OLED_I2C_CONTROL_DATA     0x40U
#define BOARD_OLED_I2C_TX_FIFO_CAPACITY    8U
#define BOARD_OLED_TIMEOUT_LOOPS     1000000U
#define BOARD_OLED_INIT_DELAY_CYCLES   800000U

static uint8_t g_board_oled_framebuffer[BOARD_OLED_FRAMEBUFFER_SIZE];
static uint8_t g_board_oled_tx_buffer[BOARD_OLED_WIDTH + 1U];
static uint8_t g_board_oled_column;
static uint8_t g_board_oled_page;

static bool controller_error(void)
{
    return (DL_I2C_getControllerStatus(I2C_OLED_INST) &
            DL_I2C_CONTROLLER_STATUS_ERROR) != 0U;
}

static board_oled_status_t wait_for_idle(void)
{
    uint32_t timeout = BOARD_OLED_TIMEOUT_LOOPS;

    while (!(DL_I2C_getControllerStatus(I2C_OLED_INST) &
             DL_I2C_CONTROLLER_STATUS_IDLE)) {
        if (timeout == 0U) {
            return BOARD_OLED_STATUS_TIMEOUT;
        }
        --timeout;
    }
    return BOARD_OLED_STATUS_OK;
}

static board_oled_status_t write_i2c(const uint8_t *data, uint16_t length)
{
    uint16_t loaded;
    uint16_t preload;
    uint32_t timeout;
    board_oled_status_t status;

    if ((data == NULL) || (length == 0U) || (length > 0x0FFFU)) {
        return BOARD_OLED_STATUS_ARGUMENT;
    }

    status = wait_for_idle();
    if (status != BOARD_OLED_STATUS_OK) {
        return status;
    }

    DL_I2C_flushControllerTXFIFO(I2C_OLED_INST);
    preload = (length < BOARD_OLED_I2C_TX_FIFO_CAPACITY) ?
              length : BOARD_OLED_I2C_TX_FIFO_CAPACITY;
    loaded = DL_I2C_fillControllerTXFIFO(I2C_OLED_INST, data, preload);
    if (loaded != preload) {
        return BOARD_OLED_STATUS_BUS_ERROR;
    }

    DL_I2C_startControllerTransfer(I2C_OLED_INST, OLED_I2C_ADDRESS,
                                   DL_I2C_CONTROLLER_DIRECTION_TX, length);
    timeout = BOARD_OLED_TIMEOUT_LOOPS;
    while (loaded < length) {
        if (controller_error()) {
            return BOARD_OLED_STATUS_BUS_ERROR;
        }
        if (!DL_I2C_isControllerTXFIFOFull(I2C_OLED_INST)) {
            DL_I2C_transmitControllerData(I2C_OLED_INST, data[loaded]);
            ++loaded;
            timeout = BOARD_OLED_TIMEOUT_LOOPS;
        } else if (timeout == 0U) {
            return BOARD_OLED_STATUS_TIMEOUT;
        } else {
            --timeout;
        }
    }

    timeout = BOARD_OLED_TIMEOUT_LOOPS;
    while (DL_I2C_getControllerStatus(I2C_OLED_INST) &
           DL_I2C_CONTROLLER_STATUS_BUSY) {
        if (controller_error()) {
            return BOARD_OLED_STATUS_BUS_ERROR;
        }
        if (timeout == 0U) {
            return BOARD_OLED_STATUS_TIMEOUT;
        }
        --timeout;
    }

    return controller_error() ? BOARD_OLED_STATUS_BUS_ERROR :
                                BOARD_OLED_STATUS_OK;
}

static board_oled_status_t write_command(const uint8_t *commands,
                                         uint8_t length)
{
    if ((commands == NULL) || (length == 0U) || (length > BOARD_OLED_WIDTH)) {
        return BOARD_OLED_STATUS_ARGUMENT;
    }

    g_board_oled_tx_buffer[0] = BOARD_OLED_I2C_CONTROL_COMMAND;
    memcpy(&g_board_oled_tx_buffer[1], commands, length);
    return write_i2c(g_board_oled_tx_buffer, (uint16_t)length + 1U);
}

static board_oled_status_t write_data_page(uint8_t page)
{
    if (page >= BOARD_OLED_PAGE_COUNT) {
        return BOARD_OLED_STATUS_ARGUMENT;
    }

    g_board_oled_tx_buffer[0] = BOARD_OLED_I2C_CONTROL_DATA;
    memcpy(&g_board_oled_tx_buffer[1],
           &g_board_oled_framebuffer[page * BOARD_OLED_WIDTH],
           BOARD_OLED_WIDTH);
    return write_i2c(g_board_oled_tx_buffer, BOARD_OLED_WIDTH + 1U);
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

    memset(g_board_oled_framebuffer, 0, sizeof(g_board_oled_framebuffer));
    g_board_oled_column = 0U;
    g_board_oled_page = 0U;
    DL_Common_delayCycles(BOARD_OLED_INIT_DELAY_CYCLES);
    status = write_command(init_commands, sizeof(init_commands));
    if (status != BOARD_OLED_STATUS_OK) {
        return status;
    }
    return board_oled_update();
}

board_oled_status_t board_oled_clear(void)
{
    memset(g_board_oled_framebuffer, 0, sizeof(g_board_oled_framebuffer));
    g_board_oled_column = 0U;
    g_board_oled_page = 0U;
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
    g_board_oled_column = column;
    g_board_oled_page = page;
    return BOARD_OLED_STATUS_OK;
}

board_oled_status_t board_oled_write_char(char value)
{
    uint8_t index;

    if ((value < (char)BOARD_OLED_FONT_FIRST_ASCII) ||
        (value > (char)BOARD_OLED_FONT_LAST_ASCII)) {
        value = '?';
    }
    if ((g_board_oled_page >= BOARD_OLED_PAGE_COUNT) ||
        (g_board_oled_column + BOARD_OLED_FONT_WIDTH >= BOARD_OLED_WIDTH)) {
        return BOARD_OLED_STATUS_ARGUMENT;
    }

    index = (uint8_t)value - BOARD_OLED_FONT_FIRST_ASCII;
    memcpy(&g_board_oled_framebuffer[g_board_oled_page * BOARD_OLED_WIDTH +
                                     g_board_oled_column],
           g_board_oled_font[index], BOARD_OLED_FONT_WIDTH);
    g_board_oled_column += BOARD_OLED_FONT_WIDTH + 1U;
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
            g_board_oled_column = 0U;
            if (g_board_oled_page + 1U >= BOARD_OLED_PAGE_COUNT) {
                return BOARD_OLED_STATUS_ARGUMENT;
            }
            ++g_board_oled_page;
            ++text;
            continue;
        }
        if (g_board_oled_column + BOARD_OLED_FONT_WIDTH >= BOARD_OLED_WIDTH) {
            g_board_oled_column = 0U;
            if (g_board_oled_page + 1U >= BOARD_OLED_PAGE_COUNT) {
                return BOARD_OLED_STATUS_ARGUMENT;
            }
            ++g_board_oled_page;
        }
        status = board_oled_write_char(*text++);
        if (status != BOARD_OLED_STATUS_OK) {
            return status;
        }
    }
    return BOARD_OLED_STATUS_OK;
}
