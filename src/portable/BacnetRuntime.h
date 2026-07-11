// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include "BacnetTypes.h"

class BacnetMonotonicClock {
public:
  virtual ~BacnetMonotonicClock() = default;
  virtual uint32_t nowMs() const = 0;
};

class BacnetDatagramTransport {
public:
  virtual ~BacnetDatagramTransport() = default;
  virtual bool begin(uint16_t localPort) = 0;
  virtual void end() = 0;
  virtual bool send(const BacnetIpEndpoint& destination,
                    const uint8_t* data,
                    size_t length) = 0;
  virtual size_t receive(uint8_t* data,
                         size_t capacity,
                         BacnetIpEndpoint& source) = 0;
};
