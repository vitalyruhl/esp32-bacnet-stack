// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetServer.h"
#include "portable/BacnetDisplayText.h"

class SmokeTransport final : public BacnetDatagramTransport {
public:
  bool begin(uint16_t) override {
    return true;
  }
  void end() override {}
  bool send(const BacnetIpEndpoint&, const uint8_t*, size_t) override {
    return true;
  }
  size_t receive(uint8_t*, size_t, BacnetIpEndpoint&) override {
    return 0;
  }
  void idle() override {}
};

int main() {
  uint8_t frame[BacnetProtocol::kWhoIsRequestSize] = {};
  const BacnetValue value{};
  SmokeTransport transport;
  BacnetServer server(transport);
  BacnetServerDevice device;
  device.deviceInstance = 1234;
  return BacnetProtocol::buildWhoIsRequest(frame, sizeof(frame)) == sizeof(frame) &&
             bacnetObjectTypePrefix(BacnetObjectType::AnalogValue)[0] == 'A' &&
             !bacnetValueInRange(value, 0.0f, 1.0f) &&
             server.begin(device) &&
             server.poll() == BacnetServerPollResult::NoDatagram
           ? 0
           : 1;
}
