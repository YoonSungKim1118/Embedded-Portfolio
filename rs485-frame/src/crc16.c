#include "crc16.h"

uint16_t crc16_modbus_update(uint16_t crc, uint8_t b)
{
    crc ^= (uint16_t)b;
    for (int i = 0; i < 8; ++i) {
        if (crc & 0x0001u) {
            crc = (uint16_t)((crc >> 1) ^ 0xA001u);
        } else {
            crc = (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

uint16_t crc16_modbus(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc = crc16_modbus_update(crc, data[i]);
    }
    return crc;
}
