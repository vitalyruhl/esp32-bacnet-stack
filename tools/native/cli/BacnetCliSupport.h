// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <cstdint>

#include "portable/BacnetRuntime.h"

enum class BacnetCliExitCode : int {
  Success = 0,
  RuntimeFailure = 1,
  Timeout = 2,
  InvalidArguments = 3,
};

bool bacnetCliParseUnsigned(const char* text, uint32_t& value);
bool bacnetCliParseIpv4(const char* text, BacnetIpEndpoint& endpoint);
const char* bacnetCliExitCodeText(BacnetCliExitCode code);
