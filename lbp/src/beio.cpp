// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include <lbp/beio.h>

namespace lbp {

uint8_t readBE8(const uint8_t *data)
{
	return data[0];
}

uint16_t readBE16(const uint8_t *data)
{
	return ((uint16_t) data[0]) << 8 | data[1];
}

uint32_t readBE32(const uint8_t *data)
{
	return ((uint32_t) data[0]) << 24
		| ((uint32_t) data[1]) << 16
		| ((uint32_t) data[2]) << 8
		| ((uint32_t) data[3]);
}

void writeBE8(uint8_t num, uint8_t *data)
{
	data[0] = num;
}

void writeBE16(uint16_t num, uint8_t *data)
{
	data[0] = (uint8_t) (num >> 8);
	data[1] = (uint8_t) num;
}

void writeBE32(uint32_t num, uint8_t *data)
{
	data[0] = (uint8_t) (num >> 24);
	data[1] = (uint8_t) (num >> 16);
	data[2] = (uint8_t) (num >> 8);
	data[3] = (uint8_t) num;
}

} // namespace lbp
