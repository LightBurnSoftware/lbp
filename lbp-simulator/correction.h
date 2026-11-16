// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include <QByteArray>

#include <cstdint>
#include <vector>

/**
 * @brief A galvo lens correction table, parsed from an .lbpcor file (file_type_correction).
 *
 * See spec.md, "Lens Correction Files". This is the validation a firmware performs at cmd_file_end.
 */
struct CorrectionTable
{
	bool valid = false;
	int32_t field_size_um = 0; // the field is square; replaces cfg_field_size while the table is applied
	int grid_size = 0;		   // nodes per side, odd
	uint8_t axis_flags = 0;	   // lbpcor_axis_*; for LightBurn, ignored here
	uint8_t composition = 0;   // lbpcor_has_*; for LightBurn, ignored here
	std::vector<int32_t> dx_nm; // grid_size * grid_size, row-major, x fastest, row 0 at -y
	std::vector<int32_t> dy_nm;

	/**
	 * @brief Parse and validate an .lbpcor file.
	 * @param detail Out: for err_invalid_header, the byte offset of the offending header field; else 0.
	 * @return 0 if valid (and this table is filled in), else err_invalid_header or err_invalid_file.
	 */
	uint16_t parse(const QByteArray &data, uint32_t &detail);
};
