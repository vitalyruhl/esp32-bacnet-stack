// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <cstdint>

class BacnetClient {
 public:
  static constexpr uint16_t kDefaultPort = 47808;

  BacnetClient() = default;

  bool begin(uint16_t localPort = kDefaultPort);
  void end();

  bool isRunning() const;
  uint16_t localPort() const;

 private:
  bool running_ = false;
  uint16_t localPort_ = kDefaultPort;
};
