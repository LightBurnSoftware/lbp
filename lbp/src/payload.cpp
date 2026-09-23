// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include <algorithm>
#include <cstring>

#include <lbp/payload.h>

namespace lbp {

CmdPayload toCmd(const MaxPayload &mp)
{
    CmdPayload c;
    c.reset(static_cast<uint16_t>(std::min<size_t>(mp.size(), CmdPayload::kCapacity)));
    std::memcpy(c.data(), mp.data(), c.size());
    return c;
}
} // namespace lbp
