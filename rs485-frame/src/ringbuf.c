#include "ringbuf.h"

static bool is_pow2(uint16_t v) { return v != 0u && (v & (uint16_t)(v - 1u)) == 0u; }

bool rb_init(ringbuf_t *rb, uint8_t *storage, uint16_t capacity)
{
    if (rb == NULL || storage == NULL || !is_pow2(capacity)) {
        return false;
    }
    rb->buf  = storage;
    rb->mask = (uint16_t)(capacity - 1u);
    rb->head = 0u;
    rb->tail = 0u;
    return true;
}

void rb_reset(ringbuf_t *rb) { rb->head = 0u; rb->tail = 0u; }

uint16_t rb_count(const ringbuf_t *rb)
{
    return (uint16_t)((rb->head - rb->tail) & rb->mask);
}

bool rb_is_empty(const ringbuf_t *rb) { return rb->head == rb->tail; }

bool rb_is_full(const ringbuf_t *rb)
{
    return (uint16_t)((rb->head + 1u) & rb->mask) == rb->tail;
}

bool rb_put(ringbuf_t *rb, uint8_t b)
{
    uint16_t next = (uint16_t)((rb->head + 1u) & rb->mask);
    if (next == rb->tail) {
        return false;                 /* full */
    }
    rb->buf[rb->head] = b;
    rb->head = next;
    return true;
}

bool rb_get(ringbuf_t *rb, uint8_t *out)
{
    if (rb->head == rb->tail) {
        return false;                 /* empty */
    }
    *out = rb->buf[rb->tail];
    rb->tail = (uint16_t)((rb->tail + 1u) & rb->mask);
    return true;
}

bool rb_peek(const ringbuf_t *rb, uint16_t offset, uint8_t *out)
{
    if (offset >= rb_count(rb)) {
        return false;
    }
    *out = rb->buf[(uint16_t)((rb->tail + offset) & rb->mask)];
    return true;
}

uint16_t rb_drop(ringbuf_t *rb, uint16_t n)
{
    uint16_t avail = rb_count(rb);
    uint16_t k     = (n < avail) ? n : avail;
    rb->tail = (uint16_t)((rb->tail + k) & rb->mask);
    return k;
}
