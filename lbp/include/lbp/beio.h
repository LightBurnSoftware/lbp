// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#ifndef LBP_BEIO_H
#define LBP_BEIO_H

#include <cstdint>

/**
 * BEIO: Big-Endian Input/Output.
 *
 * A set of helper functions for both reading values from - and writing values to -
 * big-endian data buffers.
 *
 * WARNING: These functions are not bounds-checked.
 */
namespace lbp {

/**
 * @brief Read a single 8-bit value from a big-endian data buffer.
 * @param data The data buffer. Must be at least 1 byte in length.
 * @return The resulting 8-bit value.
 */
uint8_t readBE8(const uint8_t *data);

/**
 * @brief Read a 16-bit value from a big-endian data buffer.
 * @param data The data buffer. Must be at least 2 bytes in length.
 * @return The resulting 16-bit value.
 */
uint16_t readBE16(const uint8_t *data);

/**
 * @brief Read a 32-bit value from a big-endian data buffer.
 * @param data The data buffer. Must be at least 4 bytes in length.
 * @return The resulting 32-bit value.
 */
uint32_t readBE32(const uint8_t *data);

/**
 * @brief Write a 8-bit value to a big-endian data buffer.
 * @param num The value to write.
 * @param data The data buffer. Must be at least 1 byte in length.
 */
void writeBE8(uint8_t num, uint8_t *data);

/**
 * @brief Write a 16-bit value to a big-endian data buffer.
 * @param num The value to write.
 * @param data The data buffer. Must be at least 2 bytes in length.
 */
void writeBE16(uint16_t num, uint8_t *data);

/**
 * @brief Write a 32-bit value to a big-endian data buffer.
 * @param num The value to write.
 * @param data The data buffer. Must be at least 4 bytes in length.
 */
void writeBE32(uint32_t num, uint8_t *data);

} // namespace lbp

#endif // #LBP_BEIO_H
