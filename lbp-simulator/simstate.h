// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include "vec8.h"

/** What kind of machine the simulator pretends to be. */
enum class MachineMode {
	Gantry, // (0,0) is a corner of the bed; galvo commands are not supported.
	Galvo,  // (0,0) is the centre of the marking field; galvo commands are supported.
};

struct SimState
{
	Vec8 pos;
	float laser_1_power = 0.f;
	float laser_2_power = 0.f;

	// For display
	MachineMode mode = MachineMode::Gantry;
	Vec8 min_pos;				// travel limits, micrometers
	Vec8 max_pos;
	bool guide_light = false;	// galvo guide (aiming) light on
	bool correction_on = false;	// galvo lens correction applied
};
