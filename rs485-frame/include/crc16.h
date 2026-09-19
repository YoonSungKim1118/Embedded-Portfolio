/* crc16.h — CRC-16/MODBUS
 * poly 0xA001 (reflected 0x8005), init 0xFFFF, no final xor.
 * 표에 의존하지 않는 비트 단위 구현. 256바이트 테이블을 쓸 여유가 없는
 * 소형 MCU 를 전제로 한다.
 */
#ifndef CRC16_H
#define CRC16_H

#include <stddef.h>
#include <stdint.h>

uint16_t crc16_modbus_update(uint16_t crc, uint8_t b);
uint16_t crc16_modbus(const uint8_t *data, size_t len);

#endif /* CRC16_H */
