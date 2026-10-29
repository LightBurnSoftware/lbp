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
 *
 * The result is the standard CRC-16/X-25 value: crc16("123456789") == 0x906E. Like every other field,
 * it is sent big-endian (high byte first) at the end of a message. See spec.md, "Checksum".
 */
uint16_t crc16(const uint8_t *data_p, size_t len);

/** Incremental form: crc16_end(crc16_update(crc16_begin(), a, n), ...) == crc16(a + b...). */
constexpr uint16_t crc16_begin() { return 0xffff; }
uint16_t crc16_update(uint16_t crc, const uint8_t *data_p, size_t len);
uint16_t crc16_end(uint16_t crc);

/**
 * CRC-32 as used by zlib (CRC-32/ISO-HDLC): reflected polynomial 0xEDB88320, initial value and
 * final XOR 0xFFFFFFFF. Check value: crc32("123456789") == 0xCBF43926.
 * Used for whole-file checks: cmd_file_end carries the CRC-32 of the file (see spec.md, "Receiving a File").
 */
uint32_t crc32(const uint8_t *data_p, size_t len);

/** Incremental form: crc32_end(crc32_update(crc32_begin(), a, n), ...) == crc32(a + b...). */
constexpr uint32_t crc32_begin() { return 0xFFFFFFFFu; }
uint32_t crc32_update(uint32_t crc, const uint8_t *data_p, size_t len);
constexpr uint32_t crc32_end(uint32_t crc) { return ~crc; }

} // namespace lbp

#endif // LBP_CHECKSUM_H
