// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include <lbp/message.h>
#include <lbp/parser.h>
#include <lbp/queue.h>

using CmdQueue = lbp::Queue<lbp::CmdPayload, 512>;
using OutputQueue = lbp::Queue<lbp::CmdMsg, 512>;
using WireParser = lbp::Parser<4096>;
using FileParser = lbp::Parser<4096>;

constexpr int64_t sim_step_ns = 200'000;
