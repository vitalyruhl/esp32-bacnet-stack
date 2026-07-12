// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <Arduino.h>
#include <Udp.h>

#include "BacnetClient.h"

class ArduinoMonotonicClock final : public BacnetMonotonicClock {
public:
  uint32_t nowMs() const override;
};

class ArduinoUdpDatagramTransport final : public BacnetDatagramTransport {
public:
  explicit ArduinoUdpDatagramTransport(UDP& udp) : udp_(udp) {}

  bool begin(uint16_t localPort) override;
  void end() override;
  bool send(const BacnetIpEndpoint& destination,
            const uint8_t* data,
            size_t length) override;
  size_t receive(uint8_t* data,
                 size_t capacity,
                 BacnetIpEndpoint& source) override;
  void idle() override;

private:
  UDP& udp_;
};

class ArduinoSerialLogOutput final : public BacnetLogOutput {
public:
  explicit ArduinoSerialLogOutput(Print& output = Serial) : output_(output) {}

  void log(const BacnetLogRecord& record) override;

private:
  Print& output_;
};

BacnetIpEndpoint bacnetIpEndpointFromArduino(const IPAddress& address,
                                             uint16_t port = BacnetClient::kDefaultPort);
IPAddress bacnetIpAddressFromEndpoint(const BacnetIpEndpoint& endpoint);
