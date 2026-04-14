#include <lbp/checksum.h>

uint16_t crc16(const uint8_t *data_p, uint16_t len)
{
	static constexpr uint16_t POLY = 0x8408;
	uint8_t i;
	uint16_t data;
	uint16_t crc = 0xffff;

	if (len == 0) {
		return (~crc);
	}

	do {
		for (i = 0, data = (uint16_t) 0xff & *data_p++; i < 8; i++, data >>= 1) {
			if ((crc & 0x0001) ^ (data & 0x0001))
				crc = (crc >> 1) ^ POLY;
			else
				crc >>= 1;
		}
	} while (--len);

	crc = ~crc;
	data = crc;
	crc = (crc << 8) | (data >> 8 & 0xff);

	return (crc);
}
