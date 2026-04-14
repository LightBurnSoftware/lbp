// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include <lbp/message.h>
#include <lbp/parser.h>
#include <lbp/queue.h>

constexpr size_t MaxFileSize = 65536;

using CmdQueue = lbp::Queue<lbp::CmdPayload, 512>;
using OutputQueue = lbp::Queue<lbp::CmdMsg, 512>;
using WireParser = lbp::Parser<4096>;
using FileParser = lbp::Parser<4096>;
