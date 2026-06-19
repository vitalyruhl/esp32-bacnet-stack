// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <Arduino.h>
#include <WiFiUdp.h>

#include <cstddef>
#include <cstdint>

struct BacnetIAmDevice {
  uint32_t deviceInstance = 0;
  uint32_t maxApduLengthAccepted = 0;
  uint8_t segmentationSupported = 0;
  uint16_t vendorId = 0;
};

class BacnetClient {
 public:
  static constexpr uint16_t kDefaultPort = 47808;
  static constexpr size_t kWhoIsRequestSize = 8;

  BacnetClient() = default;

  bool begin(uint16_t localPort = kDefaultPort);
  void end();

  bool isRunning() const;
  uint16_t localPort() const;

  bool sendWhoIs(IPAddress address = IPAddress(255, 255, 255, 255),
                 uint16_t port = kDefaultPort);
  bool pollIAm(BacnetIAmDevice& device);

  static size_t buildWhoIsRequest(uint8_t* buffer, size_t bufferSize);
  static bool parseIAmResponse(const uint8_t* buffer, size_t length,
                               BacnetIAmDevice& device);

 private:
  static constexpr size_t kMaxDiscoveryPacketSize = 512;

  WiFiUDP udp_;
  bool running_ = false;
  uint16_t localPort_ = kDefaultPort;
};
