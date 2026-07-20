#include "echo_queue.h"

void echo_queue_init(EchoQueue *queue)
{
    queue->head           = 0U;
    queue->tail           = 0U;
    queue->count          = 0U;
    queue->overflow_count = 0U;
}

bool echo_queue_push(EchoQueue *queue, uint8_t byte)
{
    if (queue->count == ECHO_QUEUE_CAPACITY) {
        ++queue->overflow_count;
        return false;
    }

    queue->data[queue->head] = byte;
    queue->head = (uint16_t) ((queue->head + 1U) % ECHO_QUEUE_CAPACITY);
    ++queue->count;
    return true;
}

bool echo_queue_pop(EchoQueue *queue, uint8_t *byte)
{
    if (queue->count == 0U) {
        return false;
    }

    *byte = queue->data[queue->tail];
    queue->tail = (uint16_t) ((queue->tail + 1U) % ECHO_QUEUE_CAPACITY);
    --queue->count;
    return true;
}
