#include "rs485_frame.h"
#include "crc16.h"

#include <string.h>

void rs485_rx_init(rs485_rx_t *rx, uint32_t gap_ms)
{
    memset(rx, 0, sizeof(*rx));
    rx->gap_ms = gap_ms;
}

void rs485_rx_reset(rs485_rx_t *rx)
{
    rx->raw_n = 0u;
}

/* 앞에서 n 바이트를 버린다. */
static void drop_front(rs485_rx_t *rx, uint16_t n)
{
    if (n >= rx->raw_n) {
        rx->raw_n = 0u;
        return;
    }
    memmove(rx->raw, &rx->raw[n], (size_t)(rx->raw_n - n));
    rx->raw_n = (uint16_t)(rx->raw_n - n);
}

void rs485_rx_tick(rs485_rx_t *rx, uint32_t now_ms)
{
    if (rx->gap_ms == 0u || rx->raw_n == 0u) {
        return;
    }
    if ((uint32_t)(now_ms - rx->last_byte_ms) >= rx->gap_ms) {
        rx->stats.timeouts++;
        rx->stats.discarded_bytes += rx->raw_n;
        rx->raw_n = 0u;
    }
}

/* 버퍼 앞에서 온전한 프레임 하나를 찾는다.
 * 찾으면 true 를 돌려주고 소비한 바이트를 버퍼에서 제거한다.
 * 더 받아야 하면 false 를 돌려주고 버퍼를 그대로 둔다. */
static bool try_extract(rs485_rx_t *rx)
{
    for (;;) {
        /* 1) STX 찾기 */
        if (rx->raw_n == 0u) {
            return false;
        }
        if (rx->raw[0] != RS485_STX) {
            rx->stats.discarded_bytes++;
            drop_front(rx, 1u);
            continue;
        }

        /* 2) 헤더가 다 왔는가 */
        if (rx->raw_n < 4u) {
            return false;                     /* STX ADDR CMD LEN */
        }
        uint16_t len   = rx->raw[3];
        uint16_t total = (uint16_t)(len + RS485_OVERHEAD);

        /* 3) 프레임이 다 왔는가 */
        if (rx->raw_n < total) {
            return false;
        }

        /* 4) CRC 대조 */
        uint16_t calc = crc16_modbus(&rx->raw[1], (size_t)len + 3u);
        uint16_t rxc  = (uint16_t)rx->raw[total - 2]
                      | (uint16_t)((uint16_t)rx->raw[total - 1] << 8);

        if (calc == rxc) {
            rx->frame.addr = rx->raw[1];
            rx->frame.cmd  = rx->raw[2];
            rx->frame.len  = (uint8_t)len;
            if (len != 0u) {
                memcpy(rx->frame.data, &rx->raw[4], len);
            }
            rx->stats.frames_ok++;
            drop_front(rx, total);
            return true;
        }

        /* CRC 가 틀렸다. 이 STX 는 데이터 안의 0x02 였을 수 있으므로
         * 한 바이트만 버리고 다시 훑는다. */
        rx->stats.crc_error++;
        rx->stats.discarded_bytes++;
        drop_front(rx, 1u);
    }
}

bool rs485_rx_push(rs485_rx_t *rx, uint8_t b, uint32_t now_ms)
{
    if (rx->gap_ms != 0u && rx->raw_n != 0u &&
        (uint32_t)(now_ms - rx->last_byte_ms) >= rx->gap_ms) {
        rx->stats.timeouts++;
        rx->stats.discarded_bytes += rx->raw_n;
        rx->raw_n = 0u;
    }
    rx->last_byte_ms = now_ms;

    if (rx->raw_n >= RS485_MAX_FRAME) {
        /* 여기까지 왔다면 버퍼 전체가 동기를 못 맞춘 잡음이다.
         * 앞을 한 바이트 밀어 최신 바이트를 받아들인다. */
        rx->stats.overflows++;
        rx->stats.discarded_bytes++;
        drop_front(rx, 1u);
    }
    rx->raw[rx->raw_n++] = b;

    return try_extract(rx);
}

size_t rs485_build(uint8_t *out, size_t out_cap,
                   uint8_t addr, uint8_t cmd,
                   const uint8_t *data, uint8_t len)
{
    size_t total = (size_t)len + RS485_OVERHEAD;
    if (out == NULL || out_cap < total || (len != 0u && data == NULL)) {
        return 0u;
    }

    size_t i = 0u;
    out[i++] = RS485_STX;
    out[i++] = addr;
    out[i++] = cmd;
    out[i++] = len;
    if (len != 0u) {
        memcpy(&out[i], data, len);
        i += len;
    }

    uint16_t crc = crc16_modbus(&out[1], (size_t)len + 3u);   /* ADDR..DATA */
    out[i++] = (uint8_t)(crc & 0xFFu);
    out[i++] = (uint8_t)((crc >> 8) & 0xFFu);

    return i;
}
