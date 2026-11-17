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
	return (uint16_t) ~crc;
}

uint16_t crc16(const uint8_t *data_p, size_t len)
{
	return crc16_end(crc16_update(crc16_begin(), data_p, len));
}

uint32_t crc32_update(uint32_t crc, const uint8_t *data_p, size_t len)
{
	// Bitwise form: small and table-free, for firmware with little memory.
	static constexpr uint32_t POLY = 0xEDB88320u;
	while (len--) {
		crc ^= *data_p++;
		for (int i = 0; i < 8; i++) {
			crc = (crc & 1u) ? (crc >> 1) ^ POLY : crc >> 1;
		}
	}
	return crc;
}

uint32_t crc32(const uint8_t *data_p, size_t len)
{
	return crc32_end(crc32_update(crc32_begin(), data_p, len));
}
} // namespace lbp
