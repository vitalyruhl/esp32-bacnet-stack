// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include "BacnetCliSupport.h"
#include "BacnetClient.h"

#include <cstdint>

enum class BacnetNativeReadStatus : uint8_t {
  Ack,
  Error,
  Reject,
  Abort,
  Timeout,
  DecodeError,
  SendFailed,
};

struct BacnetNativeCliOptions {
  BacnetIpEndpoint bindEndpoint;
  BacnetIpEndpoint broadcastEndpoint;
  BacnetIpEndpoint targetEndpoint;
  uint32_t deviceId = 0;
  uint32_t timeoutMs = 5000;
  bool hasBind = false;
  bool hasBroadcast = false;
  bool hasTarget = false;
  bool hasDeviceId = false;
  bool verbose = false;
};

struct BacnetObjectSelector {
  BacnetObjectId object;
};

bool bacnetNativeParseEndpoint(const char* text, BacnetIpEndpoint& endpoint);
bool bacnetNativeParseObjectSelector(const char* text, BacnetObjectSelector& selector);
bool bacnetNativeParseProperty(const char* text, BacnetPropertyId& property);
bool bacnetNativeParseObjectPropertySelector(const char* text,
                                             BacnetObjectSelector& selector,
                                             BacnetPropertyId& property);
const char* bacnetNativeObjectToken(BacnetObjectId object, char* buffer, size_t capacity);
const char* bacnetNativeReadStatusText(BacnetNativeReadStatus status);
BacnetCliExitCode bacnetNativeReadExitCode(BacnetNativeReadStatus status);

class WindowsBacnetDatagramTransport;
class WindowsMonotonicClock;

bool bacnetNativeResolveDevice(BacnetClient& client,
                               WindowsBacnetDatagramTransport& transport,
                               const BacnetNativeCliOptions& options,
                               BacnetIpEndpoint& endpoint);
BacnetNativeReadStatus bacnetNativeReadProperty(BacnetClient& client,
                                                const BacnetIpEndpoint& endpoint,
                                                BacnetPropertyRequest request,
                                                uint32_t timeoutMs,
                                                BacnetValue& value);
