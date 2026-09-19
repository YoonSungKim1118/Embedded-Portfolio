/* 호스트에서 도는 단위 시험. 외부 프레임워크를 쓰지 않는다. */
#include "crc16.h"
#include "ringbuf.h"
#include "rs485_frame.h"

#include <stdio.h>
#include <string.h>

static int g_fail = 0;
static int g_run  = 0;

#define CHECK(cond)                                                      \
    do {                                                                 \
        g_run++;                                                         \
        if (!(cond)) {                                                   \
            g_fail++;                                                    \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);     \
        }                                                                \
    } while (0)

/* ------------------------------------------------------------ ringbuf */
static void test_ringbuf(void)
{
    printf("ringbuf\n");
    uint8_t   storage[8];
    ringbuf_t rb;

    CHECK(rb_init(&rb, storage, 8u) == true);
    CHECK(rb_init(&rb, storage, 7u) == false);   /* 2의 거듭제곱이 아님 */
    CHECK(rb_init(&rb, storage, 8u) == true);

    CHECK(rb_is_empty(&rb));
    CHECK(rb_count(&rb) == 0u);

    for (uint8_t i = 0; i < 7u; ++i) {
        CHECK(rb_put(&rb, i) == true);
    }
    CHECK(rb_is_full(&rb));                       /* 한 칸은 비워 둔다 */
    CHECK(rb_put(&rb, 99u) == false);
    CHECK(rb_count(&rb) == 7u);

    uint8_t v = 0xFFu;
    CHECK(rb_peek(&rb, 0u, &v) && v == 0u);
    CHECK(rb_peek(&rb, 6u, &v) && v == 6u);
    CHECK(rb_peek(&rb, 7u, &v) == false);

    CHECK(rb_get(&rb, &v) && v == 0u);
    CHECK(rb_drop(&rb, 3u) == 3u);
    CHECK(rb_count(&rb) == 3u);
    CHECK(rb_drop(&rb, 100u) == 3u);              /* 남은 만큼만 */
    CHECK(rb_is_empty(&rb));
    CHECK(rb_get(&rb, &v) == false);

    /* 인덱스가 한 바퀴 돌아도 동작해야 한다 */
    for (int k = 0; k < 100; ++k) {
        CHECK(rb_put(&rb, (uint8_t)k));
        CHECK(rb_get(&rb, &v) && v == (uint8_t)k);
    }
}

/* -------------------------------------------------------------- crc16 */
static void test_crc16(void)
{
    printf("crc16\n");
    /* CRC-16/MODBUS 표준 검사값 : "123456789" -> 0x4B37 */
    const uint8_t vec[] = "123456789";
    CHECK(crc16_modbus(vec, 9u) == 0x4B37u);

    /* 누적 갱신과 일괄 계산이 같아야 한다 */
    uint16_t acc = 0xFFFFu;
    for (size_t i = 0; i < 9u; ++i) {
        acc = crc16_modbus_update(acc, vec[i]);
    }
    CHECK(acc == crc16_modbus(vec, 9u));
}

/* ------------------------------------------------------- build / parse */
static void feed(rs485_rx_t *rx, const uint8_t *p, size_t n,
                 uint32_t t0, int *ok_count)
{
    for (size_t i = 0; i < n; ++i) {
        if (rs485_rx_push(rx, p[i], t0)) {
            (*ok_count)++;
        }
    }
}

static void test_roundtrip(void)
{
    printf("build/parse roundtrip\n");
    uint8_t       buf[RS485_MAX_FRAME];
    const uint8_t payload[] = {0x10u, 0x20u, 0x30u, 0x40u};

    size_t n = rs485_build(buf, sizeof(buf), 0x07u, 0x33u, payload, 4u);
    CHECK(n == 4u + RS485_OVERHEAD);
    CHECK(buf[0] == RS485_STX);

    rs485_rx_t rx;
    rs485_rx_init(&rx, 0u);
    int ok = 0;
    feed(&rx, buf, n, 0u, &ok);

    CHECK(ok == 1);
    CHECK(rx.frame.addr == 0x07u);
    CHECK(rx.frame.cmd  == 0x33u);
    CHECK(rx.frame.len  == 4u);
    CHECK(memcmp(rx.frame.data, payload, 4u) == 0);
    CHECK(rx.stats.frames_ok == 1u);
    CHECK(rx.stats.crc_error == 0u);

    /* 길이 0 프레임도 유효하다 */
    n = rs485_build(buf, sizeof(buf), 0x01u, 0x02u, NULL, 0u);
    CHECK(n == RS485_OVERHEAD);
    ok = 0;
    feed(&rx, buf, n, 0u, &ok);
    CHECK(ok == 1);
    CHECK(rx.frame.len == 0u);

    /* 버퍼가 모자라면 만들지 않는다 */
    CHECK(rs485_build(buf, 3u, 0x01u, 0x02u, payload, 4u) == 0u);
}

static void test_noise_and_crc(void)
{
    printf("noise / crc error\n");
    uint8_t       buf[RS485_MAX_FRAME];
    const uint8_t payload[] = {0xAAu, 0xBBu};
    size_t        n = rs485_build(buf, sizeof(buf), 0x11u, 0x22u, payload, 2u);

    rs485_rx_t rx;
    rs485_rx_init(&rx, 10u);          /* 실제 버스처럼 무음 구간을 둔다 */
    int ok = 0;

    /* 잡음 뭉치가 지나가고, 회선이 잠깐 조용해진 뒤 프레임이 온다 */
    const uint8_t noise[] = {0xFFu, 0x00u, 0x5Au, 0x02u /* 가짜 STX */, 0x99u};
    feed(&rx, noise, sizeof(noise), 0u, &ok);
    feed(&rx, buf, n, 100u, &ok);     /* 100ms 뒤 = gap 초과 */
    CHECK(ok == 1);
    CHECK(rx.frame.addr == 0x11u);
    CHECK(rx.stats.timeouts == 1u);

    /* CRC 를 깨뜨리면 프레임이 나오지 않고 카운터가 올라간다 */
    rs485_rx_init(&rx, 0u);
    buf[n - 1] ^= 0xFFu;
    ok = 0;
    feed(&rx, buf, n, 0u, &ok);
    CHECK(ok == 0);
    CHECK(rx.stats.crc_error == 1u);
    CHECK(rx.stats.frames_ok == 0u);
}

static void test_resync_without_timer(void)
{
    printf("resync on CRC failure (no timer)\n");
    uint8_t       buf[RS485_MAX_FRAME];
    const uint8_t payload[] = {0x77u};
    size_t        n = rs485_build(buf, sizeof(buf), 0x44u, 0x55u, payload, 1u);

    /* 가짜 헤더(STX, len=0)가 앞에 붙어도, 후보 프레임이 버퍼 안에서
     * 완결되는 크기라면 타이머 없이 CRC 실패만으로 다시 동기를 맞춘다. */
    const uint8_t fake[] = {RS485_STX, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u};

    rs485_rx_t rx;
    rs485_rx_init(&rx, 0u);
    int ok = 0;
    feed(&rx, fake, sizeof(fake), 0u, &ok);
    feed(&rx, buf,  n,            0u, &ok);

    CHECK(ok == 1);
    CHECK(rx.frame.addr == 0x44u);
    CHECK(rx.frame.cmd  == 0x55u);
    CHECK(rx.stats.crc_error >= 1u);
}

static void test_stx_inside_payload(void)
{
    printf("STX inside payload\n");
    uint8_t buf[RS485_MAX_FRAME];

    /* 데이터에 0x02 가 들어 있어도 온전한 프레임은 그대로 읽혀야 한다 */
    const uint8_t payload[] = {0x02u, 0x02u, 0x10u, 0x02u};
    size_t n = rs485_build(buf, sizeof(buf), 0x21u, 0x31u, payload, 4u);

    rs485_rx_t rx;
    rs485_rx_init(&rx, 0u);
    int ok = 0;
    feed(&rx, buf, n, 0u, &ok);
    CHECK(ok == 1);
    CHECK(rx.frame.addr == 0x21u);
    CHECK(rx.frame.len  == 4u);
    CHECK(memcmp(rx.frame.data, payload, 4u) == 0);

    /* 잘린 프레임 뒤에 온전한 프레임이 이어 오는 경우.
     * 앞 조각의 길이 필드가 잡음으로 커지면 타이머 없이는 풀 수 없다.
     * 무음 구간이 있으면 복구된다 — 이것이 gap_ms 를 두는 이유다. */
    rs485_rx_init(&rx, 10u);
    ok = 0;
    feed(&rx, buf, n - 2u, 0u,   &ok);   /* CRC 를 못 채운 조각 */
    feed(&rx, buf, n,      100u, &ok);   /* 무음 뒤 온전한 프레임 */
    CHECK(ok == 1);
    CHECK(rx.frame.addr == 0x21u);
    CHECK(rx.stats.timeouts == 1u);
}

static void test_gap_timeout(void)
{
    printf("inter-byte gap timeout\n");
    uint8_t       buf[RS485_MAX_FRAME];
    const uint8_t payload[] = {1u, 2u, 3u};
    size_t        n = rs485_build(buf, sizeof(buf), 0x05u, 0x06u, payload, 3u);

    rs485_rx_t rx;
    rs485_rx_init(&rx, 10u);          /* 10ms 이상 조용하면 버린다 */

    /* 프레임 절반만 보내고 회선이 조용해진 경우 */
    for (size_t i = 0; i < 4u; ++i) {
        CHECK(rs485_rx_push(&rx, buf[i], 100u) == false);
    }
    rs485_rx_tick(&rx, 115u);
    CHECK(rx.stats.timeouts == 1u);
    CHECK(rx.raw_n == 0u);

    /* 그 뒤에 온 온전한 프레임은 정상적으로 잡혀야 한다 */
    int ok = 0;
    feed(&rx, buf, n, 200u, &ok);
    CHECK(ok == 1);
    CHECK(rx.frame.addr == 0x05u);
}

int main(void)
{
    test_ringbuf();
    test_crc16();
    test_roundtrip();
    test_noise_and_crc();
    test_resync_without_timer();
    test_stx_inside_payload();
    test_gap_timeout();

    printf("\n%d checks, %d failed\n", g_run, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
