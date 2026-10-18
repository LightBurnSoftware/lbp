#include <lbp/checksum.h>

namespace lbp {

uint16_t crc16_update(uint16_t crc, const uint8_t *data_p, size_t len)
{
	static constexpr uint16_t POLY = 0x8408;
	uint8_t i;
	uint16_t data;

	while (len--) {
		for (i = 0, data = (uint16_t) 0xff & *data_p++; i < 8; i++, data >>= 1) {
			if ((crc & 0x0001) ^ (data & 0x0001))
				crc = (crc >> 1) ^ POLY;
			else
				crc >>= 1;
		}
	}
	return crc;
}

uint16_t crc16_end(uint16_t crc)
{
	crc = ~crc;
	return (uint16_t) ((crc << 8) | (crc >> 8 & 0xff));
}

uint16_t crc16(const uint8_t *data_p, size_t len)
{
	return crc16_end(crc16_update(crc16_begin(), data_p, len));
}
} // namespace lbp
