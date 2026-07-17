// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetClient.h"
#include "BacnetDeviceSession.h"
#include "BacnetRemoteObject.h"

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
  BacnetDeviceSession session(
    client, 1234, BacnetIpEndpoint(192, 0, 2, 1, BacnetClient::kDefaultPort));
  BacnetWritePropertyOptions priorityOptions;
  priorityOptions.hasPriority = true;
  priorityOptions.priority = 8;
  if (client.sendWriteProperty(
        BacnetIpEndpoint(192, 0, 2, 1, 47808), BacnetObjectId{2, 1},
        BacnetPropertyId::PresentValue, value, priorityOptions, 1) !=
        BacnetWritePropertyPollStatus::Disabled ||
      session.object(BacnetObjectType::AnalogValue, 1).writePresentValue(value, 8) !=
        BacnetDeviceSessionWriteStatus::Disabled ||
      session.object(BacnetObjectType::AnalogValue, 1).relinquishPresentValue(8) !=
        BacnetDeviceSessionWriteStatus::Disabled || transport.sendCount != 0) {
    std::fputs("[E] disabled priority write gate sent a datagram or returned wrong status\n", stderr);
    return 1;
  }
  const BacnetPriorityRelinquishResult result =
    session.object(BacnetObjectType::AnalogValue, 1).relinquishAllPriorities();
  if (result.status != BacnetDeviceSessionWriteStatus::Disabled ||
      result.failedPriority != 1 || result.completedPriorities != 0 ||
      transport.sendCount != 0) {
    std::fputs("[E] disabled priority reset sent a datagram or returned wrong status\n", stderr);
    return 1;
  }
  BacnetPriorityResetOptions writableOptions;
  writableOptions.skipMinimumOnOffPriority = true;
  const BacnetPriorityRelinquishResult writableResult =
    session.object(BacnetObjectType::AnalogValue, 1).relinquishAllPriorities(writableOptions);
  if (writableResult.status != BacnetDeviceSessionWriteStatus::Disabled ||
      writableResult.failedPriority != 1 || writableResult.completedPriorities != 0 ||
      transport.sendCount != 0) {
    std::fputs("[E] disabled writable priority reset sent a datagram or returned wrong status\n", stderr);
    return 1;
  }
  return 0;
}
