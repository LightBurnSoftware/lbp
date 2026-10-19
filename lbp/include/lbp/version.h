// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#ifndef LBP_VERSION_H
#define LBP_VERSION_H

#include <cstdint>

#include <lbp/spec.h>

namespace lbp {

/** @return the major version: the upper byte. */
constexpr uint8_t versionMajor(uint16_t v) { return static_cast<uint8_t>(v >> 8); }

/** @return the minor version: the lower byte. */
constexpr uint8_t versionMinor(uint16_t v) { return static_cast<uint8_t>(v & 0xFF); }

/** @return a version value from its major and minor parts. */
constexpr uint16_t makeVersion(uint8_t major, uint8_t minor) { return static_cast<uint16_t>((major << 8) | minor); }

/**
 * @return true if a host speaking `host` can talk to a device speaking `device`.
 * Major versions must match. Before 1.0, minor versions must match too.
 */
constexpr bool versionsCompatible(uint16_t host, uint16_t device)
{
	return versionMajor(host) == versionMajor(device)
		&& (versionMajor(host) != 0 || versionMinor(host) == versionMinor(device));
}

static_assert(versionMajor(protocol_version) == 0 && versionMinor(protocol_version) == 9, "protocol_version is 0.9");

} // namespace lbp

#endif // LBP_VERSION_H
