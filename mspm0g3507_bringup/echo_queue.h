#ifndef ECHO_QUEUE_H
#define ECHO_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#define ECHO_QUEUE_CAPACITY (64U)

typedef struct {
    uint8_t data[ECHO_QUEUE_CAPACITY];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
    uint32_t overflow_count;
} EchoQueue;

void echo_queue_init(EchoQueue *queue);
bool echo_queue_push(EchoQueue *queue, uint8_t byte);
bool echo_queue_pop(EchoQueue *queue, uint8_t *byte);

#endif
