// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include <cassert>
#include <cstdint>

/**
 * One value per LBP axis, in micrometers. Index (idx) i, from 0, is the axis whose command flag is (1 << i)
 * (axis_x = 0x01 ... axis_v = 0x80) and whose axis id is i + 1 (axis_id_x = 1 ... axis_id_v = 8): convert with
 * idxFromAxisId and flagOfIdx. Move arguments arrive in idx order, one per flag set.
 */
struct Vec8
{
	static constexpr int kSize = 8;

	// In idx order. Use the names for a known axis, operator[] to loop over axes.
	int64_t x = 0;
	int64_t y = 0;
	int64_t z = 0;
	int64_t a = 0;
	int64_t b = 0;
	int64_t c = 0;
	int64_t u = 0;
	int64_t v = 0;

	int64_t &operator[](int idx) { return this->*member(idx); }
	int64_t operator[](int idx) const { return this->*member(idx); }

	void reset() { *this = Vec8(); }

private:
	/** @return the field for an idx. */
	static int64_t Vec8::*member(int idx)
	{
		static constexpr int64_t Vec8::*kMembers[kSize] = {
			&Vec8::x, &Vec8::y, &Vec8::z, &Vec8::a, &Vec8::b, &Vec8::c, &Vec8::u, &Vec8::v,
		};
		assert(idx >= 0 && idx < kSize);
		return kMembers[idx];
	}
};

// Axis idx values, named for readability (the LBP axis order).
constexpr int kX = 0;
constexpr int kY = 1;
constexpr int kZ = 2;
constexpr int kA = 3;
constexpr int kB = 4;
constexpr int kC = 5;
constexpr int kU = 6;
constexpr int kV = 7;

/** @return the idx of a single-axis id (axis_id_x ... axis_id_v), or -1 (0 = none, or an axis group). */
inline int idxFromAxisId(int axis_id)
{
	return (axis_id >= 1 && axis_id <= Vec8::kSize) ? axis_id - 1 : -1;
}

/** @return the command flag of an idx: 1 << idx. */
constexpr uint16_t flagOfIdx(int idx)
{
	return static_cast<uint16_t>(1u << idx);
}
