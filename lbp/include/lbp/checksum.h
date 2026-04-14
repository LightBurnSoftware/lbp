#ifndef LBP_CHECKSUM_H
#define LBP_CHECKSUM_H

#include <cstdint>

/**
 * from https://stjarnhimlen.se/snippets/crc-16.c
 *                                      16   12   5
 * this is the CCITT CRC 16 polynomial X  + X  + X  + 1.
 * This works out to be 0x1021, but the way the algorithm works
 * lets us use 0x8408 (the reverse of the bit pattern).  The high
 * bit is always assumed to be set, thus we only use 16 bits to
 * represent the 17 bit value.
 */
uint16_t crc16(const uint8_t *data_p, uint16_t len);

#endif // LBP_CHECKSUM_H
