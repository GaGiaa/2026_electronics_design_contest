#include "protocols/host_link/host_link.h"

#include <stdbool.h>
#include <string.h>

static void host_link_response(host_link_t *link, const char *text)
{
    if ((link->response_callback != NULL) && (text != NULL)) {
        link->response_callback((const uint8_t *)text, strlen(text),
                                link->response_context);
    }
}

static void host_link_process_line(host_link_t *link)
{
    link->line[link->length] = '\0';
    if (link->length == 0U) {
        return;
    }
    if (strcmp(link->line, "PING") == 0) {
        host_link_response(link, "PONG\r\n");
    } else if (strcmp(link->line, "INFO") == 0) {
        host_link_response(link, "G3507,BT_UART2,115200\r\n");
    } else if (strcmp(link->line, "STATUS") == 0) {
        host_link_response(link, "STATUS,READY\r\n");
    } else {
        host_link_response(link, "ERR,UNKNOWN\r\n");
    }
}

void host_link_init(host_link_t *link,
                    host_link_response_callback_t response_callback,
                    void *response_context)
{
    if (link == NULL) {
        return;
    }
    memset(link, 0, sizeof(*link));
    link->response_callback = response_callback;
    link->response_context = response_context;
}

void host_link_receive_byte(host_link_t *link, uint8_t byte)
{
    if (link == NULL) {
        return;
    }
    if (byte == '\n') {
        if (link->discarding_line != 0U) {
            host_link_response(link, "ERR,LONG\r\n");
        } else {
            if ((link->length > 0U) &&
                (link->line[link->length - 1U] == '\r')) {
                --link->length;
            }
            host_link_process_line(link);
        }
        link->length = 0U;
        link->discarding_line = 0U;
        return;
    }
    if (link->discarding_line != 0U) {
        return;
    }
    if (link->length >= HOST_LINK_MAX_LINE_LENGTH) {
        link->discarding_line = 1U;
        return;
    }
    link->line[link->length++] = (char)byte;
}
