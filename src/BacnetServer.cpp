// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetServer.h"

bool BacnetServer::begin(uint32_t deviceInstance, uint16_t port) {
  deviceInstance_ = deviceInstance;
  port_ = port;
  running_ = true;
  return running_;
}

void BacnetServer::end() {
  running_ = false;
}

bool BacnetServer::isRunning() const {
  return running_;
}

uint32_t BacnetServer::deviceInstance() const {
  return deviceInstance_;
}

uint16_t BacnetServer::port() const {
  return port_;
}
