// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

namespace io_example {

// Inversion affects only the physical GPIO level. A physical output that is
// out of service always receives its inactive electrical level.
inline bool ledElectricalLevel(bool presentValue, bool outOfService, bool inverted) {
  return (!outOfService && presentValue) != inverted;
}

} // namespace io_example
