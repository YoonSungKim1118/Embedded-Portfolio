/* rs485_frame.h — 바이트 스트림에서 프레임을 잘라내는 수신기
 *
 * 프레임 형식
 *   +------+------+------+------+----------+---------+
 *   | STX  | ADDR | CMD  | LEN  | DATA[..] | CRC16   |
 *   | 0x02 | 1B   | 1B   | 1B   | LEN B    | 2B (LE) |
 *   +------+------+------+------+----------+---------+
 *   CRC 범위 : ADDR ~ DATA 끝  (STX 제외), CRC-16/MODBUS
 *
 * 설계 결정
 *   1) 슬라이딩 윈도우 방식으로 만들었다.
 *      단순 상태머신은 데이터 안에 STX(0x02) 와 같은 값이 들어 있을 때
 *      가짜 헤더를 붙잡고 그 뒤의 진짜 프레임까지 삼켜 버린다.
 *      여기서는 후보 프레임의 CRC 가 틀리면 맨 앞 한 바이트만 버리고
 *      같은 버퍼를 다시 훑는다. 진짜 프레임이 잡음 바로 뒤에 붙어 와도
 *      복구된다. 대가는 프레임 한 개 크기(261B)의 버퍼 하나다.
 *   2) 프레임 경계는 길이 필드로 정하되, 회선이 조용해진 시간으로도 닫는다.
 *      Modbus RTU 의 t3.5 와 같은 개념이다. 길이 필드가 잡음으로 커졌을 때
 *      영원히 기다리지 않게 하는 안전장치다.
 *   3) 동적 할당과 콜백을 쓰지 않는다. 호출자가 상태를 소유한다.
 */
#ifndef RS485_FRAME_H
#define RS485_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RS485_STX       0x02u
#define RS485_MAX_DATA  255u
#define RS485_OVERHEAD  6u        /* STX+ADDR+CMD+LEN+CRC(2) */
#define RS485_MAX_FRAME (RS485_MAX_DATA + RS485_OVERHEAD)   /* 261 */

typedef struct {
    uint8_t addr;
    uint8_t cmd;
    uint8_t len;
    uint8_t data[RS485_MAX_DATA];
} rs485_frame_t;

typedef struct {
    uint32_t frames_ok;
    uint32_t crc_error;        /* CRC 불일치로 버린 후보 프레임 수 */
    uint32_t discarded_bytes;  /* 동기를 맞추며 버린 바이트 수 */
    uint32_t timeouts;         /* 무음 구간으로 비운 미완성 버퍼 수 */
    uint32_t overflows;        /* 버퍼가 가득 차 강제로 민 횟수 */
} rs485_stats_t;

typedef struct {
    uint8_t       raw[RS485_MAX_FRAME];
    uint16_t      raw_n;
    uint32_t      last_byte_ms;
    uint32_t      gap_ms;        /* 0 이면 무음 기반 폐기를 쓰지 않는다 */
    rs485_frame_t frame;
    rs485_stats_t stats;
} rs485_rx_t;

void rs485_rx_init(rs485_rx_t *rx, uint32_t gap_ms);
void rs485_rx_reset(rs485_rx_t *rx);

/* 한 바이트를 밀어 넣는다. 프레임이 완성되고 CRC 가 맞으면 true 이고,
 * 그때 rx->frame 이 유효하다. 다음 호출 전에 읽어야 한다.
 * now_ms 는 단조 증가 밀리초. gap_ms 가 0 이면 값은 쓰이지 않는다. */
bool rs485_rx_push(rs485_rx_t *rx, uint8_t b, uint32_t now_ms);

/* 바이트가 들어오지 않는 동안 주기적으로 불러 준다. */
void rs485_rx_tick(rs485_rx_t *rx, uint32_t now_ms);

/* 프레임을 out 에 만든다. 성공하면 총 길이, 버퍼가 작으면 0. */
size_t rs485_build(uint8_t *out, size_t out_cap,
                   uint8_t addr, uint8_t cmd,
                   const uint8_t *data, uint8_t len);

#endif /* RS485_FRAME_H */
