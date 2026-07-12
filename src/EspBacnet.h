// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include "ArduinoEspBacnet.h"
#include "BacnetDeviceSession.h"
#include "BacnetDisplayText.h"
#include "BacnetFixedTextBuffer.h"
#include "BacnetRemoteObject.h"

// Legacy combined Arduino umbrella. Prefer role-specific headers:
// BacnetClient.h plus ArduinoBacnetClient.h, or ArduinoBacnetServer.h.

namespace EspBacnet {
constexpr const char* kProjectName = "esp32-bacnet-stack";
constexpr unsigned kDefaultBacnetIpPort = 47808;
} // namespace EspBacnet
