/* ringbuf.h — 단일 생산자 / 단일 소비자 바이트 링버퍼
 *
 * ISR 이 put 하고 메인 루프가 get 하는 구조를 전제로 한다.
 * 용량은 2의 거듭제곱으로 고정해 나눗셈 없이 인덱스를 감싼다.
 * 동적 할당을 하지 않는다.
 */
#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *buf;
    uint16_t mask;             /* capacity - 1, capacity 는 2의 거듭제곱 */
    volatile uint16_t head;    /* 생산자만 쓴다 */
    volatile uint16_t tail;    /* 소비자만 쓴다 */
} ringbuf_t;

/* capacity 는 2의 거듭제곱이어야 한다. 아니면 false 를 돌려준다. */
bool     rb_init(ringbuf_t *rb, uint8_t *storage, uint16_t capacity);
void     rb_reset(ringbuf_t *rb);
uint16_t rb_count(const ringbuf_t *rb);
bool     rb_is_empty(const ringbuf_t *rb);
bool     rb_is_full(const ringbuf_t *rb);

/* 가득 차면 false. 오래된 데이터를 덮어쓰지 않는다 —
 * 통신 프레임은 뒤를 버리는 편이 앞을 깨뜨리는 것보다 복구가 쉽다. */
bool     rb_put(ringbuf_t *rb, uint8_t b);
bool     rb_get(ringbuf_t *rb, uint8_t *out);

/* 소비하지 않고 들여다본다. offset 은 tail 기준. */
bool     rb_peek(const ringbuf_t *rb, uint16_t offset, uint8_t *out);

/* n 바이트를 버린다. 실제로 버린 개수를 돌려준다. */
uint16_t rb_drop(ringbuf_t *rb, uint16_t n);

#endif /* RINGBUF_H */
