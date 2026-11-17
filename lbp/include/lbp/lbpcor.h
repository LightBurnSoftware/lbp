// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#ifndef LBP_LBPCOR_H
#define LBP_LBPCOR_H

#include <cstddef>
#include <cstdint>

/**
 * .lbpcor: reading a galvo lens correction table and correcting positions with it.
 *
 * See spec.md, "Lens Correction Files". CorrectionTable is a view over the received file's bytes: it copies
 * nothing and allocates nothing, so the file can sit in RAM or in memory-mapped flash. The bytes must stay put
 * (and unchanged) for as long as the table is used.
 *
 * All arithmetic is integer: no FPU is needed and every platform gets the same answer. Correcting a point takes
 * a few 64-bit multiplies and shifts and no divides.
 */
namespace lbp {

class CorrectionTable
{
public:
	/**
	 * @brief Check a received .lbpcor file and, if it is valid, point this table at it.
	 *
	 * Makes every receipt check from the spec, in order, so firmware can send the result straight back as the
	 * cmd_file_end NACK. On failure the table is left empty (valid() is false).
	 *
	 * @param file The whole file (its CRC-32 already checked by the transfer).
	 * @param size Its length in bytes.
	 * @param detail Out: for err_invalid_header, the byte offset of the offending header field; otherwise 0.
	 * @param max_grid The largest grid_size this firmware accepts. A larger one is err_invalid_header.
	 * @param check_cells Also refuse (err_invalid_file) a cell larger than half the field: the spec's optional
	 *        check. It reads every cell once.
	 * @return 0 if valid, else err_invalid_header or err_invalid_file.
	 */
	uint16_t parse(const uint8_t *file, size_t size, uint32_t &detail,
				   uint16_t max_grid = 0xFFFF, bool check_cells = true);

	/** @brief Forget the file. distortion() and correct() then return no correction. */
	void clear() { *this = CorrectionTable(); }

	bool valid() const { return m_cells != nullptr; }
	int32_t fieldSize() const { return m_field; }		// um; replaces cfg_field_size while the table is applied
	uint16_t gridSize() const { return m_grid; }		// nodes per side
	uint8_t axisFlags() const { return m_axis_flags; }	// lbpcor_axis_*: for LightBurn; firmware can ignore it
	uint8_t composition() const { return m_composition; } // lbpcor_has_*: for LightBurn; firmware can ignore it

	/**
	 * @brief Read one node of the grid.
	 * @param col 0 .. gridSize()-1, from -x.
	 * @param row 0 .. gridSize()-1, from -y.
	 * @param dx_nm, dy_nm Out: the measured distortion at that node, in nanometers.
	 */
	void node(uint16_t col, uint16_t row, int32_t &dx_nm, int32_t &dy_nm) const;

	/**
	 * @brief The distortion at a position, interpolated bilinearly between the four surrounding nodes.
	 *
	 * Positions are wire coordinates: micrometers from the field centre. A position outside the field takes the
	 * distortion at the nearest point on its edge (it is clamped, not extrapolated). Without a table, (0, 0).
	 *
	 * Accuracy, against exact arithmetic: within 1 nm, plus 2/1048576 of the largest difference between
	 * neighbouring nodes (about 0.2 nm when they differ by 100 um). Exact at the nodes. (For fields over 2^25 um,
	 * 33 m, a little looser.)
	 */
	void distortion(int32_t x_um, int32_t y_um, int32_t &dx_nm, int32_t &dy_nm) const;

	/**
	 * @brief Where to aim the mirrors so the beam lands on (x_um, y_um): the position minus its distortion.
	 *
	 * The result is in nanometers, so the correction keeps its full resolution through to the firmware's own
	 * conversion to mirror units.
	 */
	void correct(int32_t x_um, int32_t y_um, int64_t &x_nm, int64_t &y_nm) const;

private:
	// Grid coordinate along one axis: the cell (0 .. m_grid-2) and the fraction across it (0 .. 1 << frac_bits).
	void locate(int32_t pos_um, uint32_t &cell, uint32_t &frac) const;

	const uint8_t *m_cells = nullptr; // the first cell, in the caller's file
	int32_t m_field = 0;
	uint16_t m_grid = 0;
	uint8_t m_axis_flags = 0;
	uint8_t m_composition = 0;
	uint64_t m_scale = 0; // grid units per um, fixed point: see locate()
};

} // namespace lbp

#endif // LBP_LBPCOR_H
