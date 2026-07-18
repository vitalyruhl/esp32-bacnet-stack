// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <cstdint>

#include "portable/BacnetProtocol.h"
#include "portable/BacnetRuntime.h"

struct BacnetServerDevice {
  uint32_t deviceInstance = 0;
  uint16_t vendorId = 0;
  uint32_t maxApduLengthAccepted = 1476;
  uint8_t segmentationSupported = 3;
};

enum class BacnetServerPollResult : uint8_t {
  NotRunning,
  NoDatagram,
  Ignored,
  Malformed,
  IAmSent,
  RejectSent,
  SendFailed,
};

class BacnetServer {
public:
  static constexpr uint16_t kDefaultPort = 47808;
  static constexpr uint8_t kRejectReasonUnrecognizedService = 9;
  static constexpr size_t kMaxDatagramSize = 1476;

  BacnetServer() = default;
  explicit BacnetServer(BacnetDatagramTransport& transport);

  bool setTransport(BacnetDatagramTransport& transport);
  bool begin(const BacnetServerDevice& configuredDevice,
             uint16_t portValue = kDefaultPort);
  bool begin(uint32_t deviceInstanceValue, uint16_t portValue = kDefaultPort);
  void end();

  bool isRunning() const;
  const BacnetServerDevice& device() const;
  uint32_t deviceInstance() const;
  uint16_t port() const;
  BacnetServerPollResult poll();

private:
  bool sendIAm(const BacnetIpEndpoint& destination);
  bool sendReject(const BacnetIpEndpoint& destination, uint8_t invokeId);

  BacnetDatagramTransport* transport_ = nullptr;
  bool running_ = false;
  BacnetServerDevice device_;
  uint16_t port_ = kDefaultPort;
};
