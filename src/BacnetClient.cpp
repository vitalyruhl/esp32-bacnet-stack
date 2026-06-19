// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetClient.h"

bool BacnetClient::begin(uint16_t localPort) {
  localPort_ = localPort;
  running_ = true;
  return running_;
}

void BacnetClient::end() {
  running_ = false;
}

bool BacnetClient::isRunning() const {
  return running_;
}

uint16_t BacnetClient::localPort() const {
  return localPort_;
}
