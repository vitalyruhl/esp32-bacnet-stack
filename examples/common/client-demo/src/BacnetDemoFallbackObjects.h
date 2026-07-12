// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <portable/BacnetTypes.h>

#include <stdint.h>

BacnetObjectId bacnetDemoFallbackAnalogObject(uint32_t instance);
BacnetObjectId bacnetDemoFallbackBinaryObject(uint32_t instance);
BacnetObjectId bacnetDemoFallbackMultiStateObject(uint32_t instance);
