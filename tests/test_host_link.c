#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "protocols/host_link/host_link.h"

typedef struct {
    char response[128];
    size_t length;
} response_capture_t;

static void capture_response(const uint8_t *data, size_t length, void *context)
{
    response_capture_t *capture = (response_capture_t *)context;

    assert(length < sizeof(capture->response));
    memcpy(capture->response, data, length);
    capture->response[length] = '\0';
    capture->length = length;
}

static void feed(host_link_t *link, const char *text)
{
    while (*text != '\0') {
        host_link_receive_byte(link, (uint8_t)*text++);
    }
}

static void test_ping_accepts_crlf(void)
{
    host_link_t link;
    response_capture_t capture = {0};

    host_link_init(&link, capture_response, &capture);
    feed(&link, "PING\r\n");

    assert(strcmp(capture.response, "PONG\r\n") == 0);
}

static void test_info_status_and_lf(void)
{
    host_link_t link;
    response_capture_t capture = {0};

    host_link_init(&link, capture_response, &capture);
    feed(&link, "INFO\nSTATUS\n");
    assert(strcmp(capture.response, "STATUS,READY\r\n") == 0);
    assert(capture.length == strlen("STATUS,READY\r\n"));
}

static void test_unknown_command_returns_error(void)
{
    host_link_t link;
    response_capture_t capture = {0};

    host_link_init(&link, capture_response, &capture);
    feed(&link, "MOVE\r\n");

    assert(strcmp(capture.response, "ERR,UNKNOWN\r\n") == 0);
}

static void test_empty_line_is_ignored(void)
{
    host_link_t link;
    response_capture_t capture = {0};

    host_link_init(&link, capture_response, &capture);
    feed(&link, "\r\n");

    assert(capture.length == 0U);
}

static void test_long_line_returns_error_and_recovers(void)
{
    host_link_t link;
    response_capture_t capture = {0};
    size_t index;

    host_link_init(&link, capture_response, &capture);
    for (index = 0U; index < HOST_LINK_MAX_LINE_LENGTH + 1U; ++index) {
        host_link_receive_byte(&link, (uint8_t)'X');
    }
    host_link_receive_byte(&link, (uint8_t)'\n');
    assert(strcmp(capture.response, "ERR,LONG\r\n") == 0);

    feed(&link, "PING\n");
    assert(strcmp(capture.response, "PONG\r\n") == 0);
}

int main(void)
{
    test_ping_accepts_crlf();
    test_info_status_and_lf();
    test_unknown_command_returns_error();
    test_empty_line_is_ignored();
    test_long_line_returns_error_and_recovers();
    return 0;
}
