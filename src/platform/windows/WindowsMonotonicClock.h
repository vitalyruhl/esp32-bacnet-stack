// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include "portable/BacnetRuntime.h"

class WindowsMonotonicClock final : public BacnetMonotonicClock {
public:
  uint32_t nowMs() const override;
};
