// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include <cstdint>

/** X, Y, Z, U in micrometers. */
struct Vec4
{
	int32_t x = 0;
	int32_t y = 0;
	int32_t z = 0;
	int32_t u = 0;

	/** Default constructor. All Zeros. */
	Vec4() = default;
	Vec4(int32_t x, int32_t y, int32_t z, int32_t u);

	/** @return True if any of the four values are non-zero. */
	explicit operator bool() const;
	void reset();
};

Vec4 operator+(const Vec4 &a, const Vec4 &b);
Vec4 operator-(const Vec4 &a, const Vec4 &b);
bool operator==(const Vec4 &a, const Vec4 &b);
bool operator!=(const Vec4 &a, const Vec4 &b);
Vec4 operator*(const Vec4 &vec, int32_t m);
Vec4 operator/(const Vec4 &vec, int32_t d);

Vec4 clamp(const Vec4 &vec, const Vec4 &limit);
