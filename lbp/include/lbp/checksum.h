#ifndef LBP_CHECKSUM_H
#define LBP_CHECKSUM_H

#include <cstddef>
#include <cstdint>

namespace lbp {
/**
 * from https://stjarnhimlen.se/snippets/crc-16.c
 *                                      16   12   5
 * this is the CCITT CRC 16 polynomial X  + X  + X  + 1.
 * This works out to be 0x1021, but the way the algorithm works
 * lets us use 0x8408 (the reverse of the bit pattern).  The high
 * bit is always assumed to be set, thus we only use 16 bits to
 * represent the 17 bit value.
 */
uint16_t crc16(const uint8_t *data_p, size_t len);

/** Incremental form: crc16_end(crc16_update(crc16_begin(), a, n), ...) == crc16(a + b...). */
constexpr uint16_t crc16_begin() { return 0xffff; }
uint16_t crc16_update(uint16_t crc, const uint8_t *data_p, size_t len);
uint16_t crc16_end(uint16_t crc);

} // namespace lbp

#endif // LBP_CHECKSUM_H
