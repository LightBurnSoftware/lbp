// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "lbpcor.h"

#include <lbp/beio.h>
#include <lbp/spec.h>

#include <cstring>

namespace lbpexamples {

using namespace lbp; // the spec's constants and the big-endian readers

namespace {

// Fractions across a cell are fixed point with this many bits: 1 << frac_bits is the whole cell.
constexpr unsigned frac_bits = 20;
constexpr int64_t frac_one = int64_t(1) << frac_bits;
constexpr int64_t frac_half = frac_one >> 1;

// m_scale holds grid units per um with frac_bits + scale_bits of fraction. 46 bits in all keeps every product in
// locate() below 2^62 for any grid_size and field_size (see there), and still leaves the scale exact enough that
// positions at the nodes land exactly on them for fields up to 2^25 um (33 m).
constexpr unsigned scale_bits = 26;

int32_t readCell(const uint8_t *cell)
{
	return static_cast<int32_t>(readBE32(cell));
}

// a + (b - a) * f, rounded to the nearest nanometer. a and b are within int32, f within 0 .. frac_one, so each
// product is below 2^51. (Right shifts of negative values are arithmetic on every compiler that matters, and
// guaranteed from C++20.)
int64_t lerp(int64_t a, int64_t b, int64_t f)
{
	return (a * (frac_one - f) + b * f + frac_half) >> frac_bits;
}

int64_t magnitude(int32_t v)
{
	return v < 0 ? -int64_t(v) : int64_t(v);
}

} // namespace

uint16_t CorrectionTable::parse(const uint8_t *file, size_t size, uint32_t &detail,
								uint16_t max_grid, bool check_cells)
{
	clear();
	detail = 0;

	if (file == nullptr || size < lbpcor_header_size) {
		return err_invalid_file; // too short to hold a header
	}

	// Header: check each field, reporting the first bad one by its offset.
	if (std::memcmp(file + lbpcor_offset_magic, lbpcor_magic, sizeof(lbpcor_magic)) != 0) {
		detail = lbpcor_offset_magic;
		return err_invalid_header;
	}
	if (readBE16(file + lbpcor_offset_version) != lbpcor_version) {
		detail = lbpcor_offset_version;
		return err_invalid_header;
	}
	const uint16_t header_size = readBE16(file + lbpcor_offset_header_size);
	if (header_size < lbpcor_header_size) {
		detail = lbpcor_offset_header_size;
		return err_invalid_header;
	}
	const int32_t field = static_cast<int32_t>(readBE32(file + lbpcor_offset_field_size));
	if (field <= 0) {
		detail = lbpcor_offset_field_size;
		return err_invalid_header;
	}
	// Odd, so a node sits at the field centre.
	const uint16_t grid = readBE16(file + lbpcor_offset_grid_size);
	if (grid < 3 || (grid % 2) == 0 || grid > max_grid) {
		detail = lbpcor_offset_grid_size;
		return err_invalid_header;
	}
	if (file[lbpcor_offset_cell_format] != lbpcor_cell_int32_nm) {
		detail = lbpcor_offset_cell_format;
		return err_invalid_header;
	}
	// axis_flags and composition are LightBurn's; the reserved bytes, and any header bytes beyond these, are
	// ignored.

	// Length, in 64 bits: a 65535-node grid is larger than a 32-bit size_t can count.
	const uint64_t nodes = uint64_t(grid) * grid;
	if (uint64_t(size) != header_size + nodes * 8) {
		return err_invalid_file;
	}

	const uint8_t *cells = file + header_size;

	// The optional magnitude check: a cell larger than half the field is a broken table, most likely one written
	// in micrometers.
	if (check_cells) {
		const int64_t limit_nm = int64_t(field) * 500;
		for (uint64_t i = 0; i < nodes * 2; i++) {
			if (magnitude(readCell(cells + i * 4)) > limit_nm) {
				return err_invalid_file;
			}
		}
	}

	m_cells = cells;
	m_field = field;
	m_grid = grid;
	m_axis_flags = file[lbpcor_offset_axis_flags];
	m_composition = file[lbpcor_offset_composition];

	// Grid units per um: (grid - 1) / field. locate() works in half-micrometers so an odd field's centre is
	// whole, so this is (grid - 1) / (2 * field), rounded up so a position exactly on a node never falls short of
	// it. Below 2^62 / (2 * field).
	const uint64_t num = uint64_t(grid - 1) << (frac_bits + scale_bits);
	const uint64_t den = uint64_t(field) * 2;
	m_scale = (num + den - 1) / den;
	return 0;
}

void CorrectionTable::node(uint16_t col, uint16_t row, int32_t &dx_nm, int32_t &dy_nm) const
{
	const uint8_t *cell = m_cells + (size_t(row) * m_grid + col) * 8;
	dx_nm = readCell(cell);
	dy_nm = readCell(cell + 4);
}

void CorrectionTable::locate(int32_t pos_um, uint32_t &cell, uint32_t &frac) const
{
	// Half-micrometers from the field's -edge, clamped to the field: 0 .. 2 * field, at most 2^32.
	int64_t v = int64_t(pos_um) * 2 + m_field;
	if (v < 0) {
		v = 0;
	} else if (v > int64_t(m_field) * 2) {
		v = int64_t(m_field) * 2;
	}

	// v * m_scale is at most (grid - 1) << 46 plus 2 * field (from rounding m_scale up): below 2^62.
	const uint64_t u = (uint64_t(v) * m_scale) >> scale_bits; // grid units, frac_bits of fraction
	const uint32_t last = m_grid - 2;						   // the last cell
	cell = uint32_t(u >> frac_bits);
	if (cell > last) {
		// The +edge itself (or a hair past it, from rounding): the far side of the last cell.
		cell = last;
		frac = uint32_t(frac_one);
	} else {
		frac = uint32_t(u & (frac_one - 1));
	}
}

void CorrectionTable::distortion(int32_t x_um, int32_t y_um, int32_t &dx_nm, int32_t &dy_nm) const
{
	if (!valid()) {
		dx_nm = dy_nm = 0;
		return;
	}

	uint32_t col, fx, row, fy;
	locate(x_um, col, fx);
	locate(y_um, row, fy);

	int32_t x00, y00, x10, y10, x01, y01, x11, y11;
	node(col, row, x00, y00);
	node(col + 1, row, x10, y10);
	node(col, row + 1, x01, y01);
	node(col + 1, row + 1, x11, y11);

	// Along x on both rows, then along y between them. Each step rounds to the nearest nanometer.
	dx_nm = int32_t(lerp(lerp(x00, x10, fx), lerp(x01, x11, fx), fy));
	dy_nm = int32_t(lerp(lerp(y00, y10, fx), lerp(y01, y11, fx), fy));
}

void CorrectionTable::correct(int32_t x_um, int32_t y_um, int64_t &x_nm, int64_t &y_nm) const
{
	int32_t dx, dy;
	distortion(x_um, y_um, dx, dy);
	x_nm = int64_t(x_um) * 1000 - dx;
	y_nm = int64_t(y_um) * 1000 - dy;
}

} // namespace lbpexamples
