#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "echo_queue.h"

static void test_fifo_order(void)
{
    EchoQueue queue;
    uint8_t byte;

    echo_queue_init(&queue);
    assert(echo_queue_push(&queue, 0x31U));
    assert(echo_queue_push(&queue, 0x32U));
    assert(echo_queue_push(&queue, 0x33U));
    assert(echo_queue_pop(&queue, &byte) && byte == 0x31U);
    assert(echo_queue_pop(&queue, &byte) && byte == 0x32U);
    assert(echo_queue_pop(&queue, &byte) && byte == 0x33U);
    assert(!echo_queue_pop(&queue, &byte));
}

static void test_full_queue_counts_overflow(void)
{
    EchoQueue queue;
    uint8_t byte;
    uint16_t index;

    echo_queue_init(&queue);
    for (index = 0U; index < ECHO_QUEUE_CAPACITY; ++index) {
        assert(echo_queue_push(&queue, (uint8_t) index));
    }
    assert(!echo_queue_push(&queue, 0xFFU));
    assert(queue.overflow_count == 1U);
    assert(echo_queue_pop(&queue, &byte) && byte == 0U);
}

static void test_wraparound_preserves_order(void)
{
    EchoQueue queue;
    uint8_t byte;
    uint16_t index;

    echo_queue_init(&queue);
    for (index = 0U; index < ECHO_QUEUE_CAPACITY; ++index) {
        assert(echo_queue_push(&queue, (uint8_t) index));
    }
    for (index = 0U; index < 8U; ++index) {
        assert(echo_queue_pop(&queue, &byte) && byte == (uint8_t) index);
    }
    for (index = 0U; index < 8U; ++index) {
        assert(echo_queue_push(&queue, (uint8_t) (0x80U + index)));
    }
    for (index = 8U; index < ECHO_QUEUE_CAPACITY; ++index) {
        assert(echo_queue_pop(&queue, &byte) && byte == (uint8_t) index);
    }
    for (index = 0U; index < 8U; ++index) {
        assert(echo_queue_pop(&queue, &byte) && byte == (uint8_t) (0x80U + index));
    }
    assert(!echo_queue_pop(&queue, &byte));
}

static void test_empty_queue_rejects_pop(void)
{
    EchoQueue queue;
    uint8_t byte = 0U;

    echo_queue_init(&queue);
    assert(!echo_queue_pop(&queue, &byte));
}

int main(void)
{
    test_fifo_order();
    test_full_queue_counts_overflow();
    test_wraparound_preserves_order();
    test_empty_queue_rejects_pop();
    puts("PASS: 4 echo queue tests");
    return 0;
}
