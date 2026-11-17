// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

// Tests for the example .lbpcor reader (lbpcor.h): every receipt check, and the fixed-point interpolation held to
// a plain double reference.

#include "lbpcor.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <vector>

#include <lbp/beio.h>
#include <lbp/spec.h>

namespace {

// A cell's (dx, dy) in nm, given its column and row.
using CellFn = std::function<void(int col, int row, int32_t &dx, int32_t &dy)>;

std::vector<uint8_t> makeLbpCor(int32_t field_um, uint16_t grid, const CellFn &cell, uint16_t header_size = 24)
{
	std::vector<uint8_t> f(header_size + size_t(grid) * grid * 8, 0);
	std::memcpy(f.data(), lbp::lbpcor_magic, sizeof(lbp::lbpcor_magic));
	lbp::writeBE16(lbp::lbpcor_version, &f[lbp::lbpcor_offset_version]);
	lbp::writeBE16(header_size, &f[lbp::lbpcor_offset_header_size]);
	lbp::writeBE32(uint32_t(field_um), &f[lbp::lbpcor_offset_field_size]);
	lbp::writeBE16(grid, &f[lbp::lbpcor_offset_grid_size]);
	f[lbp::lbpcor_offset_axis_flags] = lbp::lbpcor_axis_swap;
	f[lbp::lbpcor_offset_composition] = lbp::lbpcor_has_scale | lbp::lbpcor_has_residual;
	f[lbp::lbpcor_offset_cell_format] = lbp::lbpcor_cell_int32_nm;
	for (int row = 0; row < grid; row++) {
		for (int col = 0; col < grid; col++) {
			int32_t dx = 0, dy = 0;
			cell(col, row, dx, dy);
			uint8_t *c = &f[header_size + (size_t(row) * grid + col) * 8];
			lbp::writeBE32(uint32_t(dx), c);
			lbp::writeBE32(uint32_t(dy), c + 4);
		}
	}
	return f;
}

// The spec's interpolation, written plainly in double: the reference the fixed-point version is held to.
void referenceCorrection(const lbpexamples::CorrectionTable &t, double x_um, double y_um, double &dx, double &dy)
{
	const int n = t.gridSize();
	auto locate = [&](double pos, int &cell, double &frac) {
		double u = (pos + t.fieldSize() / 2.0) * (n - 1) / t.fieldSize();
		u = std::fmin(std::fmax(u, 0.0), double(n - 1));
		cell = std::min(int(std::floor(u)), n - 2);
		frac = u - cell;
	};
	int col, row;
	double fx, fy;
	locate(x_um, col, fx);
	locate(y_um, row, fy);
	int32_t x00, y00, x10, y10, x01, y01, x11, y11;
	t.node(col, row, x00, y00);
	t.node(col + 1, row, x10, y10);
	t.node(col, row + 1, x01, y01);
	t.node(col + 1, row + 1, x11, y11);
	dx = (x00 * (1 - fx) + x10 * fx) * (1 - fy) + (x01 * (1 - fx) + x11 * fx) * fy;
	dy = (y00 * (1 - fx) + y10 * fx) * (1 - fy) + (y01 * (1 - fx) + y11 * fx) * fy;
}

double largestStep(const lbpexamples::CorrectionTable &t);

// The documented accuracy: 1 nm of rounding, plus the fraction's resolution (1/2^20 of a cell, along each axis)
// times the largest difference between neighbouring nodes. Past 2^25 um, rounding the scale up adds up to
// 2 * field / 2^26 more of those units.
double tolerance(const lbpexamples::CorrectionTable &t)
{
	const double units = 1.0 + 2.0 * t.fieldSize() / double(1 << 26);
	return 1.0 + 2.0 * units * largestStep(t) / double(1 << 20) + 1e-6;
}

// The largest difference between neighbouring nodes, in nm: the fraction's resolution error scales with it.
double largestStep(const lbpexamples::CorrectionTable &t)
{
	double worst = 0;
	for (int row = 0; row < t.gridSize(); row++) {
		for (int col = 0; col < t.gridSize(); col++) {
			int32_t dx, dy, nx, ny;
			t.node(col, row, dx, dy);
			if (col + 1 < t.gridSize()) {
				t.node(col + 1, row, nx, ny);
				worst = std::fmax(worst, std::fmax(std::fabs(double(nx) - dx), std::fabs(double(ny) - dy)));
			}
			if (row + 1 < t.gridSize()) {
				t.node(col, row + 1, nx, ny);
				worst = std::fmax(worst, std::fmax(std::fabs(double(nx) - dx), std::fabs(double(ny) - dy)));
			}
		}
	}
	return worst;
}

uint32_t nextRandom(uint32_t &state)
{
	state = state * 1664525u + 1013904223u;
	return state;
}

// Random positions across (and a little beyond) the field, against the double reference.
void checkAgainstReference(const lbpexamples::CorrectionTable &t, uint32_t seed, int count = 20000)
{
	const double limit = tolerance(t);
	const int64_t span = int64_t(t.fieldSize()) * 3 / 2; // 25% past each edge
	for (int i = 0; i < count; i++) {
		const int32_t x = int32_t(int64_t(nextRandom(seed) % uint32_t(span)) - span / 2);
		const int32_t y = int32_t(int64_t(nextRandom(seed) % uint32_t(span)) - span / 2);
		int32_t dx, dy;
		double rx, ry;
		t.correction(x, y, dx, dy);
		referenceCorrection(t, x, y, rx, ry);
		if (std::fabs(dx - rx) > limit || std::fabs(dy - ry) > limit) {
			std::cout << "lbpcor: field " << t.fieldSize() << " grid " << t.gridSize() << " at (" << x << ", " << y
					  << "): (" << dx << ", " << dy << ") vs (" << rx << ", " << ry << "), tolerance " << limit
					  << std::endl;
			assert(false);
		}
	}
}

void testCorrectionTable()
{
	using lbpexamples::CorrectionTable;
	uint32_t detail = 0;

	// A realistic table: pincushion growing to about 2 mm at the corners, plus up to +-20 um of measured noise.
	auto pincushion = [](int32_t field, uint16_t grid) {
		return [field, grid](int col, int row, int32_t &dx, int32_t &dy) {
			const double c = (grid - 1) / 2.0;
			const double x = (col - c) / c, y = (row - c) / c; // -1 .. 1
			const double k = 1e6 * field / 100000.0;		  // nm at the corners, for a 100 mm field
			uint32_t s = uint32_t(row * 7919 + col * 104729);
			dx = int32_t(k * x * (y * y) + double(nextRandom(s) % 40001) - 20000);
			dy = int32_t(k * y * (x * x) + double(nextRandom(s) % 40001) - 20000);
		};
	};

	// Receipt checks, each reporting the field it found wrong.
	{
		const std::vector<uint8_t> good = makeLbpCor(100000, 65, pincushion(100000, 65));
		CorrectionTable t;
		assert(t.parse(good.data(), good.size(), detail) == 0 && detail == 0 && t.valid());
		assert(t.fieldSize() == 100000 && t.gridSize() == 65);
		assert(t.axisFlags() == lbp::lbpcor_axis_swap);
		assert(t.composition() == (lbp::lbpcor_has_scale | lbp::lbpcor_has_residual));
		assert(good.size() == 33824);

		auto refused = [&](std::vector<uint8_t> f, uint16_t error, uint32_t offset, uint16_t max_grid = 0xFFFF) {
			CorrectionTable bad;
			uint32_t d = 0xDEAD;
			const bool ok = bad.parse(f.data(), f.size(), d, max_grid) == error && d == offset && !bad.valid();
			return ok;
		};
		auto with = [&](size_t at, uint8_t value) {
			std::vector<uint8_t> f = good;
			f[at] = value;
			return f;
		};

		assert(refused(with(lbp::lbpcor_offset_magic + 3, 'X'), lbp::err_invalid_header, lbp::lbpcor_offset_magic));
		assert(refused(with(lbp::lbpcor_offset_version + 1, 2), lbp::err_invalid_header, lbp::lbpcor_offset_version));
		assert(refused(with(lbp::lbpcor_offset_header_size + 1, 20), lbp::err_invalid_header,
					   lbp::lbpcor_offset_header_size));
		assert(refused(with(lbp::lbpcor_offset_field_size, 0x80), lbp::err_invalid_header,
					   lbp::lbpcor_offset_field_size)); // negative
		assert(refused(with(lbp::lbpcor_offset_grid_size + 1, 64), lbp::err_invalid_header,
					   lbp::lbpcor_offset_grid_size)); // even
		assert(refused(with(lbp::lbpcor_offset_grid_size + 1, 1), lbp::err_invalid_header,
					   lbp::lbpcor_offset_grid_size)); // too small
		assert(refused(good, lbp::err_invalid_header, lbp::lbpcor_offset_grid_size, 33)); // over this firmware's limit
		assert(refused(with(lbp::lbpcor_offset_cell_format, 2), lbp::err_invalid_header,
					   lbp::lbpcor_offset_cell_format));

		std::vector<uint8_t> shorter = good;
		shorter.pop_back();
		assert(refused(shorter, lbp::err_invalid_file, 0));
		assert(refused(std::vector<uint8_t>(good.begin(), good.begin() + 10), lbp::err_invalid_file, 0));

		// Ignored: LightBurn's flags and the reserved bytes.
		std::vector<uint8_t> f = with(21, 0xFF);
		f[23] = 0x55;
		f[lbp::lbpcor_offset_axis_flags] = 0xFF;
		f[lbp::lbpcor_offset_composition] = 0xFF;
		CorrectionTable t2;
		assert(t2.parse(f.data(), f.size(), detail) == 0);

		// A longer header from a later minor revision: skipped using header_size.
		const std::vector<uint8_t> longer = makeLbpCor(100000, 65, pincushion(100000, 65), 32);
		assert(t2.parse(longer.data(), longer.size(), detail) == 0);
		int32_t a, b, c, e;
		t.node(10, 20, a, b);
		t2.node(10, 20, c, e);
		assert(a == c && b == e);

		// A cell larger than half the field (here, a table written in micrometers for a 1 mm field, but with one
		// cell scaled as nm): refused only when the firmware makes the optional check.
		const std::vector<uint8_t> wild = makeLbpCor(1000, 3, [](int col, int row, int32_t &dx, int32_t &dy) {
			dx = (col == 1 && row == 1) ? 500001 : 0;
			dy = 0;
		});
		assert(t2.parse(wild.data(), wild.size(), detail) == lbp::err_invalid_file && !t2.valid());
		assert(t2.parse(wild.data(), wild.size(), detail, 0xFFFF, false) == 0);
	}

	// No table: no correction.
	{
		CorrectionTable t;
		int32_t dx = 1, dy = 1;
		t.correction(1234, -5678, dx, dy);
		assert(dx == 0 && dy == 0);
		int64_t x, y;
		t.correct(1234, -5678, x, y);
		assert(x == 1234000 && y == -5678000);
	}

	// Exact at every node, including fields near the 2^25 um limit of that guarantee.
	for (int32_t field : {96000, 64 * 524287}) {
		const uint16_t grid = 65;
		const std::vector<uint8_t> f = makeLbpCor(field, grid, pincushion(field, grid));
		CorrectionTable t;
		assert(t.parse(f.data(), f.size(), detail, 0xFFFF, false) == 0);
		const int32_t step = field / (grid - 1);
		for (int row = 0; row < grid; row++) {
			for (int col = 0; col < grid; col++) {
				int32_t dx, dy, nx, ny;
				t.correction((col - 32) * step, (row - 32) * step, dx, dy);
				t.node(col, row, nx, ny);
				assert(dx == nx && dy == ny);
			}
		}
	}

	// Between nodes, against the double reference: even and odd fields, coarse and fine grids, the smallest grid.
	{
		struct Case {
			int32_t field;
			uint16_t grid;
		};
		uint32_t seed = 1;
		for (Case c : {Case{100000, 65}, Case{78821, 65}, Case{300000, 33}, Case{110001, 3}, Case{1000, 255}}) {
			const std::vector<uint8_t> f = makeLbpCor(c.field, c.grid, pincushion(c.field, c.grid));
			CorrectionTable t;
			assert(t.parse(f.data(), f.size(), detail, 0xFFFF, false) == 0);
			checkAgainstReference(t, seed++);
		}
	}

	// Extremes: the largest field, cells at both int32 limits, positions at both int32 limits. Nothing overflows.
	{
		const int32_t field = 0x7FFFFFFF;
		const std::vector<uint8_t> f = makeLbpCor(field, 3, [](int col, int row, int32_t &dx, int32_t &dy) {
			dx = ((col + row) % 2) ? INT32_MAX : INT32_MIN;
			dy = ((col + row) % 2) ? INT32_MIN : INT32_MAX;
		});
		CorrectionTable t;
		assert(t.parse(f.data(), f.size(), detail, 0xFFFF, false) == 0);
		checkAgainstReference(t, 99);
		for (int32_t x : {INT32_MIN, -1, 0, 1, INT32_MAX}) {
			for (int32_t y : {INT32_MIN, -1, 0, 1, INT32_MAX}) {
				int32_t dx, dy;
				double rx, ry;
				t.correction(x, y, dx, dy);
				referenceCorrection(t, x, y, rx, ry);
				assert(std::fabs(dx - rx) <= tolerance(t));
				assert(std::fabs(dy - ry) <= tolerance(t));
			}
		}
	}

	// Outside the field: the correction at the nearest point on the edge. correct() subtracts it from the
	// unclamped position.
	{
		const std::vector<uint8_t> f = makeLbpCor(100000, 65, pincushion(100000, 65));
		CorrectionTable t;
		assert(t.parse(f.data(), f.size(), detail) == 0);
		int32_t ex, ey, ox, oy;
		t.correction(50000, -12345, ex, ey);
		t.correction(80000, -12345, ox, oy);
		assert(ex == ox && ey == oy);
		t.correction(-50000, -50000, ex, ey);
		t.correction(-90000, -70000, ox, oy);
		assert(ex == ox && ey == oy);

		int64_t x, y;
		t.correct(80000, -12345, x, y);
		t.correction(80000, -12345, ox, oy);
		assert(x == 80000000 - ox && y == -12345000 - oy);
	}
}

} // namespace

int main()
{
	std::cout << "lbpcor example test: start" << std::endl;
	testCorrectionTable();
	std::cout << "lbpcor example test: success" << std::endl;
	return 0;
}
