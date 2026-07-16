// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetClient.h"
#include "BacnetDeviceSession.h"
#include "BacnetWriteHil.h"

#include <cstdio>
#include <cstring>

namespace {

bool expect(bool condition, const char* text) {
  if (!condition) {
    std::fprintf(stderr, "[E] %s\n", text);
  }
  return condition;
}

class FakeClock final : public BacnetMonotonicClock {
public:
  uint32_t nowMs() const override { return now; }
  uint32_t now = 0;
};

class FakeTransport final : public BacnetDatagramTransport {
public:
  bool begin(uint16_t) override { return true; }
  void end() override {}
  bool send(const BacnetIpEndpoint&, const uint8_t* data, size_t length) override {
    ++sendCount;
    if (sendCount <= 16 && length <= sizeof(sent[0])) {
      std::memcpy(sent[sendCount - 1], data, length);
      sentSize[sendCount - 1] = length;
    }
    return true;
  }
  size_t receive(uint8_t* data, size_t capacity, BacnetIpEndpoint&) override {
    if (next == count || capacity < responseSize[next]) {
      return 0;
    }
    const size_t size = responseSize[next];
    std::memcpy(data, response[next++], size);
    return size;
  }
  void idle() override { ++clock->now; }
  bool queue(const uint8_t* data, size_t size) {
    if (count == 32 || size > sizeof(response[0])) {
      return false;
    }
    std::memcpy(response[count], data, size);
    responseSize[count++] = size;
    return true;
  }

  FakeClock* clock = nullptr;
  unsigned sendCount = 0;
  uint8_t sent[16][64] = {};
  size_t sentSize[16] = {};
  uint8_t response[32][128] = {};
  size_t responseSize[32] = {};
  size_t count = 0;
  size_t next = 0;
};

size_t buildReadAck(uint8_t* buffer, uint8_t invokeId, BacnetObjectId object,
                    BacnetPropertyId property, uint32_t arrayIndex,
                    const BacnetValue& value) {
  uint8_t encoded[64] = {};
  const size_t encodedSize = BacnetProtocol::encodeApplicationValue(
    encoded, sizeof(encoded), value);
  if (encodedSize == 0) {
    return 0;
  }
  size_t offset = 0;
  buffer[offset++] = 0x81;
  buffer[offset++] = 0x0A;
  buffer[offset++] = 0;
  buffer[offset++] = 0;
  buffer[offset++] = 0x01;
  buffer[offset++] = 0;
  buffer[offset++] = 0x30;
  buffer[offset++] = invokeId;
  buffer[offset++] = 0x0C;
  buffer[offset++] = 0x0C;
  buffer[offset++] = static_cast<uint8_t>(object.type >> 2);
  buffer[offset++] = static_cast<uint8_t>(object.type << 6 | object.instance >> 16);
  buffer[offset++] = static_cast<uint8_t>(object.instance >> 8);
  buffer[offset++] = static_cast<uint8_t>(object.instance);
  buffer[offset++] = 0x19;
  buffer[offset++] = static_cast<uint8_t>(property);
  if (arrayIndex != kBacnetNoArrayIndex) {
    buffer[offset++] = 0x29;
    buffer[offset++] = static_cast<uint8_t>(arrayIndex);
  }
  buffer[offset++] = 0x3E;
  std::memcpy(buffer + offset, encoded, encodedSize);
  offset += encodedSize;
  buffer[offset++] = 0x3F;
  buffer[2] = static_cast<uint8_t>(offset >> 8);
  buffer[3] = static_cast<uint8_t>(offset);
  return offset;
}

size_t buildReadError(uint8_t* buffer, uint8_t invokeId) {
  const uint8_t bytes[] = {
    0x81, 0x0A, 0x00, 0x0D, 0x01, 0x00, 0x50, invokeId, 0x0C,
    0x91, 0x02, 0x91, 40,
  };
  std::memcpy(buffer, bytes, sizeof(bytes));
  return sizeof(bytes);
}

size_t buildWriteAck(uint8_t* buffer, uint8_t invokeId) {
  const uint8_t bytes[] = {
    0x81, 0x0A, 0x00, 0x0A, 0x01, 0x00, 0x20, invokeId, 0x0F, 0x0F,
  };
  std::memcpy(buffer, bytes, sizeof(bytes));
  return sizeof(bytes);
}

bool testValueSelection() {
  FakeTransport transport;
  FakeClock clock;
  transport.clock = &clock;
  BacnetClient client(transport, &clock);
  client.begin();
  BacnetDeviceSession session(
    client, 1234, BacnetIpEndpoint(192, 0, 2, 1, BacnetClient::kDefaultPort));
  BacnetRemoteObject object = session.object(BacnetObjectType::BinaryValue, 7);
  BacnetValue current;
  current.type = BacnetValueType::Real;
  current.realValue = 3.0F;
  BacnetValue selected;
  if (!expect(bacnetWriteHilSelectTemporaryValue(
                object, BacnetObjectType::AnalogValue, current, 3, selected) &&
                selected.type == BacnetValueType::Real && selected.realValue == 12.5F,
              "analog fixed-value selection")) {
    return false;
  }
  current.type = BacnetValueType::Enumerated;
  current.unsignedValue = 0;
  if (!expect(bacnetWriteHilSelectTemporaryValue(
                object, BacnetObjectType::BinaryValue, current, 3, selected) &&
                selected.unsignedValue == 1,
              "binary value inverse selection")) {
    return false;
  }
  current.type = BacnetValueType::Enumerated;
  current.unsignedValue = 1;
  BacnetRemoteObject multiStateObject =
    session.object(BacnetObjectType::MultiStateValue, 7);
  BacnetValue states;
  states.type = BacnetValueType::Unsigned;
  states.unsignedValue = 3;
  uint8_t response[128] = {};
  const size_t size = buildReadAck(response, 1, multiStateObject.objectId(),
                                   BacnetPropertyId::NumberOfStates,
                                   kBacnetNoArrayIndex, states);
  BacnetValue decoded;
  const BacnetPropertyRequest request{
    multiStateObject.objectId(), BacnetPropertyId::NumberOfStates};
  if (!expect(BacnetProtocol::parseReadPropertyAck(response, size, 1, request,
                                                   decoded),
              "number-of-states ACK decoding") ||
      !expect(transport.queue(response, size), "queue number-of-states direct")) {
    return false;
  }
  const BacnetDeviceSessionReadStatus statesStatus =
    multiStateObject.readProperty(BacnetPropertyId::NumberOfStates, decoded, 3);
  if (!expect(statesStatus == BacnetDeviceSessionReadStatus::Ack &&
                decoded.unsignedValue == 3,
              "number-of-states direct read") ||
      !expect(transport.queue(response, buildReadAck(
                response, 2, multiStateObject.objectId(),
                BacnetPropertyId::NumberOfStates, kBacnetNoArrayIndex, states)),
              "queue number-of-states selection") ||
      !expect(bacnetWriteHilSelectTemporaryValue(
                multiStateObject, BacnetObjectType::MultiStateValue, current, 3, selected) &&
                selected.unsignedValue == 2,
              "multi-state alternate selection")) {
    return false;
  }
  return expect(transport.sendCount == 2,
                "multi-state selection reads number-of-states through the session");
}

bool testSuccessfulBinaryRun() {
  FakeTransport transport;
  FakeClock clock;
  transport.clock = &clock;
  BacnetClient client(transport, &clock);
  client.begin();
  BacnetDeviceSession session(
    client, 1234, BacnetIpEndpoint(192, 0, 2, 1, BacnetClient::kDefaultPort));
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::BinaryValue), 7};
  BacnetValue off;
  off.type = BacnetValueType::Enumerated;
  off.unsignedValue = 0;
  BacnetValue on = off;
  on.unsignedValue = 1;
  BacnetValue nullValue;
  nullValue.type = BacnetValueType::Null;
  uint8_t response[128] = {};
  const struct Read {
    BacnetPropertyId property;
    uint32_t index;
    BacnetValue value;
  } reads[] = {
    {BacnetPropertyId::PresentValue, kBacnetNoArrayIndex, off},
    {BacnetPropertyId::PriorityArray, 8, nullValue},
    {BacnetPropertyId::PriorityArray, 16, off},
  };
  for (uint8_t invoke = 1; invoke <= 3; ++invoke) {
    if (!expect(transport.queue(response, buildReadAck(
                  response, invoke, object, reads[invoke - 1].property,
                  reads[invoke - 1].index, reads[invoke - 1].value)),
                "queue initial read")) {
      return false;
    }
  }
  if (!expect(transport.queue(response, buildWriteAck(response, 4)),
              "queue priority write ACK")) {
    return false;
  }
  const Read activeReads[] = {
    {BacnetPropertyId::PresentValue, kBacnetNoArrayIndex, on},
    {BacnetPropertyId::PriorityArray, 8, on},
    {BacnetPropertyId::PriorityArray, 16, off},
  };
  for (uint8_t invoke = 5; invoke <= 7; ++invoke) {
    if (!expect(transport.queue(response, buildReadAck(
                  response, invoke, object, activeReads[invoke - 5].property,
                  activeReads[invoke - 5].index, activeReads[invoke - 5].value)),
                "queue active read")) {
      return false;
    }
  }
  if (!expect(transport.queue(response, buildWriteAck(response, 8)),
              "queue relinquish ACK")) {
    return false;
  }
  const Read resumedReads[] = {
    {BacnetPropertyId::PriorityArray, 8, nullValue},
    {BacnetPropertyId::PriorityArray, 16, off},
    {BacnetPropertyId::PresentValue, kBacnetNoArrayIndex, off},
  };
  for (uint8_t invoke = 9; invoke <= 11; ++invoke) {
    if (!expect(transport.queue(response, buildReadAck(
                  response, invoke, object, resumedReads[invoke - 9].property,
                  resumedReads[invoke - 9].index, resumedReads[invoke - 9].value)),
                "queue resumed read")) {
      return false;
    }
  }
  const BacnetWriteHilTarget target{BacnetObjectType::BinaryValue, 7, false};
  const BacnetWriteHilResult result = bacnetWriteHilRunTarget(session, target, {8, 3});
  return expect(result.succeeded() && transport.sendCount == 11 &&
                  transport.sent[3][transport.sentSize[3] - 2] == 0x49 &&
                  transport.sent[3][transport.sentSize[3] - 1] == 8 &&
                  transport.sent[7][18] == 0x00,
                "binary HIL write/readback/relinquish flow");
}

bool testReadbackFailureRelinquishes() {
  FakeTransport transport;
  FakeClock clock;
  transport.clock = &clock;
  BacnetClient client(transport, &clock);
  client.begin();
  BacnetDeviceSession session(
    client, 1234, BacnetIpEndpoint(192, 0, 2, 1, BacnetClient::kDefaultPort));
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::AnalogValue), 7};
  BacnetValue original;
  original.type = BacnetValueType::Real;
  original.realValue = 3.0F;
  BacnetValue nullValue;
  nullValue.type = BacnetValueType::Null;
  uint8_t response[128] = {};
  if (!expect(transport.queue(response, buildReadAck(response, 1, object,
                                                      BacnetPropertyId::PresentValue,
                                                      kBacnetNoArrayIndex, original)) &&
                  transport.queue(response, buildReadAck(response, 2, object,
                                                         BacnetPropertyId::PriorityArray,
                                                         8, nullValue)) &&
                  transport.queue(response, buildReadAck(response, 3, object,
                                                         BacnetPropertyId::PriorityArray,
                                                         16, original)) &&
                  transport.queue(response, buildWriteAck(response, 4)),
              "queue setup for cleanup test")) {
    return false;
  }
  BacnetValue temporary = original;
  temporary.realValue = 12.5F;
  if (!expect(transport.queue(response, buildReadAck(response, 5, object,
                                                      BacnetPropertyId::PresentValue,
                                                      kBacnetNoArrayIndex, temporary)) &&
                  transport.queue(response, buildReadError(response, 6)) &&
                  transport.queue(response, buildWriteAck(response, 7)),
              "queue readback failure and cleanup ACK")) {
    return false;
  }
  const BacnetWriteHilTarget target{BacnetObjectType::AnalogValue, 7, false};
  const BacnetWriteHilResult result = bacnetWriteHilRunTarget(session, target, {8, 3});
  return expect(result.stage == BacnetWriteHilStage::Readback &&
                  result.cleanupAttempted &&
                  result.relinquishStatus == BacnetDeviceSessionWriteStatus::Ack &&
                  transport.sendCount == 7 && transport.sent[6][18] == 0x00,
                "readback failure relinquishes active priority");
}

bool testInvalidConfigurationSendsNothing() {
  FakeTransport transport;
  FakeClock clock;
  transport.clock = &clock;
  BacnetClient client(transport, &clock);
  client.begin();
  BacnetDeviceSession session(
    client, 1234, BacnetIpEndpoint(192, 0, 2, 1, BacnetClient::kDefaultPort));
  const BacnetWriteHilTarget invalid{BacnetObjectType::AnalogValue, 0, false};
  const BacnetWriteHilResult result = bacnetWriteHilRunTarget(session, invalid, {8, 3});
  return expect(result.stage == BacnetWriteHilStage::InvalidConfiguration &&
                  transport.sendCount == 0,
                "invalid HIL configuration sends no datagram");
}

}  // namespace

int main() {
  return testValueSelection() && testSuccessfulBinaryRun() &&
         testReadbackFailureRelinquishes() && testInvalidConfigurationSendsNothing()
           ? 0
           : 1;
}
