// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetClient.h"
#include "BacnetDeviceSession.h"
#include "BacnetRemoteObject.h"

#include <cstdio>
#include <cstring>

namespace {

class FakeTransport final : public BacnetDatagramTransport {
public:
  bool begin(uint16_t) override { return true; }
  void end() override {}
  bool send(const BacnetIpEndpoint&, const uint8_t* data, size_t length) override {
    ++sendCount;
    if (length <= sizeof(lastPacket)) {
      std::memcpy(lastPacket, data, length);
      lastPacketSize = length;
    }
    return true;
  }
  size_t receive(uint8_t* data, size_t capacity, BacnetIpEndpoint&) override {
    if (responseSize == 0 || capacity < responseSize) {
      return 0;
    }
    std::memcpy(data, response, responseSize);
    const size_t size = responseSize;
    responseSize = 0;
    return size;
  }
  void idle() override {}

  unsigned sendCount = 0;
  uint8_t lastPacket[64] = {};
  size_t lastPacketSize = 0;
  uint8_t response[16] = {};
  size_t responseSize = 0;
};

bool expect(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "[E] %s\n", message);
  }
  return condition;
}

void queueWriteAck(FakeTransport& transport, uint8_t invokeId) {
  const uint8_t ack[] = {
    0x81, 0x0A, 0x00, 0x0A, 0x01, 0x00, 0x20, invokeId, 0x0F, 0x0F,
  };
  std::memcpy(transport.response, ack, sizeof(ack));
  transport.responseSize = sizeof(ack);
}

}  // namespace

int main() {
  FakeTransport transport;
  BacnetClient client(transport);
  if (!expect(client.begin(), "fake client begin")) {
    return 1;
  }
  BacnetDeviceSession session(
    client, 1234, BacnetIpEndpoint(192, 0, 2, 1, BacnetClient::kDefaultPort));
  BacnetRemoteObject object = session.object(BacnetObjectType::AnalogValue, 1);
  BacnetValue value;
  value.type = BacnetValueType::Real;
  value.realValue = 1.0F;
  BacnetWritePropertyOptions priorityOptions;
  priorityOptions.hasPriority = true;
  priorityOptions.priority = 8;

  if (!expect(client.sendWriteProperty(
                BacnetIpEndpoint(192, 0, 2, 1, BacnetClient::kDefaultPort),
                object.objectId(), BacnetPropertyId::PresentValue, value,
                priorityOptions, 99) == BacnetWritePropertyPollStatus::Disabled &&
                  object.writePresentValue(value, 8, 1) ==
                    BacnetDeviceSessionWriteStatus::Disabled &&
                  object.relinquishPresentValue(8, 1) ==
                    BacnetDeviceSessionWriteStatus::Disabled &&
                  transport.sendCount == 0,
              "priority writes are disabled without sending a datagram")) {
    return 1;
  }

  BacnetPriorityResetOptions writableOptions;
  writableOptions.skipMinimumOnOffPriority = true;
  const BacnetPriorityRelinquishResult strict = object.relinquishAllPriorities(1);
  const BacnetPriorityRelinquishResult writable =
    object.relinquishAllPriorities(writableOptions, 1);
  if (!expect(strict.status == BacnetDeviceSessionWriteStatus::Disabled &&
                  strict.failedPriority == 1 && writable.status ==
                    BacnetDeviceSessionWriteStatus::Disabled &&
                  writable.failedPriority == 1 && transport.sendCount == 0,
              "priority resets are disabled without sending a datagram")) {
    return 1;
  }

  queueWriteAck(transport, 1);
  if (!expect(session.writeProperty(object.objectId(), BacnetPropertyId::PresentValue,
                                    value, 1) == BacnetDeviceSessionWriteStatus::Ack &&
                  transport.sendCount == 1 && transport.lastPacket[8] == 1,
              "normal write remains enabled and priority rejection used no invoke ID")) {
    return 1;
  }
  return 0;
}
