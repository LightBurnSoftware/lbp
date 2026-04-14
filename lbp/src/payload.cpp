// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include <algorithm>

#include <lbp/payload.h>

namespace lbp {

CmdPayload toCmd(const MaxPayload &mp)
{
	CmdPayload c;
	c.reset(mp.size());
	memcpy(c.data(), mp.data(), std::min(mp.size(), mp.kCapacity));
	return c;
}
} // namespace lbp
