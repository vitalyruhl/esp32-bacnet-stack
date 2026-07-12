// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetClient.h"

#include <cstdio>

class FakeTransport final : public BacnetDatagramTransport {
public:
  bool begin(uint16_t) override { return true; }
  void end() override {}
  bool send(const BacnetIpEndpoint&, const uint8_t*, size_t) override { ++sendCount; return true; }
  size_t receive(uint8_t*, size_t, BacnetIpEndpoint&) override { return 0; }
  void idle() override {}
  unsigned sendCount = 0;
};

int main() {
  FakeTransport transport;
  BacnetClient client(transport);
  client.begin();
  BacnetValue value;
  value.type = BacnetValueType::Null;
  const BacnetWritePropertyPollStatus status = client.sendWriteProperty(
    BacnetIpEndpoint(192, 0, 2, 1, 47808),
    BacnetPropertyRequest{BacnetObjectId{2, 1}, BacnetPropertyId::PresentValue},
    value, 1);
  if (status != BacnetWritePropertyPollStatus::Disabled || transport.sendCount != 0) {
    std::fputs("[E] disabled write gate sent a datagram or returned wrong status\n", stderr);
    return 1;
  }
  return 0;
}
