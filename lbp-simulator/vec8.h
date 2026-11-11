// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include <cassert>
#include <cstdint>

/**
 * One value per LBP axis, in micrometers, indexed by axis slot. Slot i is the axis whose command flag is (1 << i)
 * (axis_x = 0x01 ... axis_v = 0x80) and whose cfg index is i + 1 (axis_index_x = 1 ... axis_index_v = 8). Move
 * arguments arrive in slot order, one per flag set.
 */
struct Vec8
{
	static constexpr int kSize = 8;

	// In slot order. Use the names for a known axis, operator[] to loop over axes.
	int64_t x = 0;
	int64_t y = 0;
	int64_t z = 0;
	int64_t a = 0;
	int64_t b = 0;
	int64_t c = 0;
	int64_t u = 0;
	int64_t v = 0;

	int64_t &operator[](int slot) { return this->*member(slot); }
	int64_t operator[](int slot) const { return this->*member(slot); }

	void reset() { *this = Vec8(); }

private:
	/** @return the field for a slot. */
	static int64_t Vec8::*member(int slot)
	{
		static constexpr int64_t Vec8::*kMembers[kSize] = {
			&Vec8::x, &Vec8::y, &Vec8::z, &Vec8::a, &Vec8::b, &Vec8::c, &Vec8::u, &Vec8::v,
		};
		assert(slot >= 0 && slot < kSize);
		return kMembers[slot];
	}
};

// Axis slots, named for readability (the LBP axis order).
constexpr int kX = 0;
constexpr int kY = 1;
constexpr int kZ = 2;
constexpr int kA = 3;
constexpr int kB = 4;
constexpr int kC = 5;
constexpr int kU = 6;
constexpr int kV = 7;

/** @return the slot of a cfg axis index (axis_index_x ... axis_index_v), or -1. */
inline int slotOfIndex(int axis_index)
{
	return (axis_index >= 1 && axis_index <= Vec8::kSize) ? axis_index - 1 : -1;
}

/** @return the command flag of a slot: 1 << slot. */
constexpr uint16_t flagOfSlot(int slot)
{
	return static_cast<uint16_t>(1u << slot);
}
