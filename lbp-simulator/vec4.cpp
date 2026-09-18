// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "vec4.h"

Vec4::Vec4(int64_t x, int64_t y, int64_t z, int64_t u)
	: x(x)
	, y(y)
	, z(z)
	, u(u)
{
	// Empty
}

Vec4 operator+(const Vec4 &a, const Vec4 &b)
{
	return Vec4(a.x + b.x, a.y + b.y, a.z + b.z, a.u + b.u);
}

Vec4 operator-(const Vec4 &a, const Vec4 &b)
{
	return Vec4(a.x - b.x, a.y - b.y, a.z - b.z, a.u - b.u);
}

bool operator==(const Vec4 &a, const Vec4 &b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z && a.u == b.u;
}

bool operator!=(const Vec4 &a, const Vec4 &b)
{
	return !(a == b);
}

Vec4::operator bool() const
{
	return x != 0 || y != 0 || z != 0 || u != 0;
}

Vec4 operator*(const Vec4 &vec, int64_t m)
{
	Vec4 product;
	product.x = vec.x * m;
	product.y = vec.y * m;
	product.z = vec.z * m;
	product.u = vec.u * m;
	return product;
}

Vec4 operator/(const Vec4 &vec, int64_t d)
{
	Vec4 product;
	product.x = vec.x / d;
	product.y = vec.y / d;
	product.z = vec.z / d;
	product.u = vec.u / d;
	return product;
}

void Vec4::reset()
{
	x = 0;
	y = 0;
	z = 0;
	u = 0;
}

static int64_t clamp(int64_t lower, int64_t value, int64_t upper)
{
	value = value > upper ? upper : value;
	value = value < lower ? lower : value;
	return value;
}

Vec4 clamp(const Vec4 &vec, const Vec4 &limit)
{
	Vec4 result;
	result.x = clamp(0, vec.x, limit.x);
	result.y = clamp(0, vec.y, limit.y);
	result.z = clamp(0, vec.z, limit.z);
	result.u = clamp(0, vec.u, limit.u);
	return result;
}
