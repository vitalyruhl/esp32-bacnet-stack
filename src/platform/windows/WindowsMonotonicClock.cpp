// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "platform/windows/WindowsMonotonicClock.h"

#include <chrono>

uint32_t WindowsMonotonicClock::nowMs() const {
  const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
  return static_cast<uint32_t>(
    std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}
