// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <cstdint>

class BacnetServer {
public:
  static constexpr uint16_t kDefaultPort = 47808;

  BacnetServer() = default;

  bool begin(uint32_t deviceInstanceValue, uint16_t portValue = kDefaultPort);
  void end();

  bool isRunning() const;
  uint32_t deviceInstance() const;
  uint16_t port() const;

private:
  bool running_ = false;
  uint32_t deviceInstance_ = 0;
  uint16_t port_ = kDefaultPort;
};
