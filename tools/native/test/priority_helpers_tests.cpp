// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetClient.h"
#include "BacnetDeviceSession.h"
#include "BacnetRemoteObject.h"

#include <cstdio>
#include <cstring>

namespace {

bool expect(bool condition, const char* text) {
  if (!condition) {
    std::fprintf(stderr, "[E] %s\n", text);
  }
  return condition;
}

bool expectBytes(const uint8_t* actual,
                 size_t actualSize,
                 const uint8_t* expected,
                 size_t expectedSize,
                 const char* text) {
  return expect(actualSize == expectedSize &&
                  std::memcmp(actual, expected, expectedSize) == 0,
                text);
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
    if (length <= sizeof(lastPacket)) {
      std::memcpy(lastPacket, data, length);
      lastPacketSize = length;
    }
    if (sendCount <= kMaxResponses && length <= sizeof(sentPackets[0])) {
      std::memcpy(sentPackets[sendCount - 1], data, length);
      sentPacketSizes[sendCount - 1] = length;
    }
    return sendResult;
  }

  size_t receive(uint8_t* data, size_t capacity, BacnetIpEndpoint&) override {
    if (nextResponse >= responseCount || capacity < responseSizes[nextResponse]) {
      return 0;
    }
    const size_t size = responseSizes[nextResponse];
    std::memcpy(data, responses[nextResponse++], size);
    return size;
  }

  void idle() override {
    if (clock != nullptr) {
      ++clock->now;
    }
  }

  bool queueResponse(const uint8_t* data, size_t length) {
    if (nextResponse == responseCount) {
      responseCount = 0;
      nextResponse = 0;
    }
    if (responseCount == kMaxResponses || length > sizeof(responses[0])) {
      return false;
    }
    std::memcpy(responses[responseCount], data, length);
    responseSizes[responseCount++] = length;
    return true;
  }

  static constexpr size_t kMaxResponses = 20;
  FakeClock* clock = nullptr;
  bool sendResult = true;
  unsigned sendCount = 0;
  uint8_t lastPacket[600] = {};
  size_t lastPacketSize = 0;
  uint8_t sentPackets[kMaxResponses][600] = {};
  size_t sentPacketSizes[kMaxResponses] = {};
  uint8_t responses[kMaxResponses][128] = {};
  size_t responseSizes[kMaxResponses] = {};
  size_t responseCount = 0;
  size_t nextResponse = 0;
};

size_t buildReadAck(uint8_t* buffer,
                    uint8_t invokeId,
                    BacnetObjectId object,
                    BacnetPropertyId property,
                    uint32_t arrayIndex,
                    const uint8_t* value,
                    size_t valueLength) {
  size_t offset = 0;
  buffer[offset++] = 0x81;
  buffer[offset++] = 0x0A;
  buffer[offset++] = 0;
  buffer[offset++] = 0;
  buffer[offset++] = 0x01;
  buffer[offset++] = 0x00;
  buffer[offset++] = 0x30;
  buffer[offset++] = invokeId;
  buffer[offset++] = 0x0C;
  buffer[offset++] = 0x0C;
  buffer[offset++] = static_cast<uint8_t>(object.type >> 8);
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
  std::memcpy(buffer + offset, value, valueLength);
  offset += valueLength;
  buffer[offset++] = 0x3F;
  buffer[2] = static_cast<uint8_t>(offset >> 8);
  buffer[3] = static_cast<uint8_t>(offset);
  return offset;
}

size_t buildPriorityArrayAck(uint8_t* buffer,
                             uint8_t invokeId,
                             BacnetObjectId object,
                             const BacnetValue& slotEight,
                             const BacnetValue& slotSixteen) {
  uint8_t values[96] = {};
  size_t valuesLength = 0;
  for (size_t slot = 0; slot < BacnetPriorityArray::kSlotCount; ++slot) {
    BacnetValue value;
    if (slot == 7) {
      value = slotEight;
    } else if (slot == 15) {
      value = slotSixteen;
    } else {
      value.type = BacnetValueType::Null;
    }
    const size_t encoded = BacnetProtocol::encodeApplicationValue(
      values + valuesLength, sizeof(values) - valuesLength, value);
    if (encoded == 0) {
      return 0;
    }
    valuesLength += encoded;
  }
  return buildReadAck(buffer,
                      invokeId,
                      object,
                      BacnetPropertyId::PriorityArray,
                      kBacnetNoArrayIndex,
                      values,
                      valuesLength);
}

size_t buildReadError(uint8_t* buffer,
                      uint8_t invokeId,
                      uint8_t errorClass,
                      uint8_t errorCode) {
  const uint8_t response[] = {
    0x81, 0x0A, 0x00, 0x0D, 0x01, 0x00, 0x50, invokeId, 0x0C,
    0x91, errorClass, 0x91, errorCode,
  };
  std::memcpy(buffer, response, sizeof(response));
  return sizeof(response);
}

size_t buildTerminalResponse(uint8_t* buffer, uint8_t apduType, uint8_t invokeId) {
  const uint8_t response[] = {0x81, 0x0A, 0x00, 0x08, 0x01, 0x00, apduType, invokeId};
  std::memcpy(buffer, response, sizeof(response));
  return sizeof(response);
}

size_t buildWriteAck(uint8_t* buffer, uint8_t invokeId) {
  const uint8_t response[] = {
    0x81, 0x0A, 0x00, 0x0A, 0x01, 0x00, 0x20, invokeId, 0x0F, 0x0F,
  };
  std::memcpy(buffer, response, sizeof(response));
  return sizeof(response);
}

bool testReadHelpers() {
  FakeTransport transport;
  FakeClock clock;
  transport.clock = &clock;
  BacnetClient client(transport, &clock);
  BacnetDeviceSession session(
    client, 1234, BacnetIpEndpoint(192, 0, 2, 1, BacnetClient::kDefaultPort));
  BacnetRemoteObject object = session.object(BacnetObjectType::AnalogValue, 7);
  if (!expect(client.begin(), "fake client begin")) {
    return false;
  }
  const BacnetObjectId objectId = object.objectId();
  const uint8_t realValue[] = {0x44, 0x3F, 0x80, 0x00, 0x00};
  uint8_t response[128] = {};
  BacnetValue value;

  BacnetValue slotEight;
  slotEight.type = BacnetValueType::Real;
  slotEight.realValue = 12.5F;
  BacnetValue slotSixteen;
  slotSixteen.type = BacnetValueType::Enumerated;
  slotSixteen.unsignedValue = 2;
  size_t responseSize = buildPriorityArrayAck(
    response, 1, objectId, slotEight, slotSixteen);
  BacnetPriorityArray priorityArray;
  if (!expect(responseSize != 0 && transport.queueResponse(response, responseSize), "queue full priority array ACK") ||
      !expect(object.readPriorityArray(priorityArray, 3) == BacnetPropertyReadStatus::Ack,
              "full priority array ACK") ||
      !expect(priorityArray.present[7] && priorityArray.present[15],
              "full priority array slot presence") ||
      !expect(priorityArray.slots[7].type == BacnetValueType::Real &&
              priorityArray.slots[7].realValue == 12.5F,
              "full priority array slot 8") ||
      !expect(priorityArray.slots[15].type == BacnetValueType::Enumerated &&
              priorityArray.slots[15].unsignedValue == 2,
              "full priority array slot 16")) {
    return false;
  }

  responseSize = buildReadAck(response, 2, objectId,
                              BacnetPropertyId::PriorityArray, 16,
                              realValue, sizeof(realValue));
  if (!expect(transport.queueResponse(response, responseSize), "queue indexed priority array ACK") ||
      !expect(object.readPriorityArray(value, 3, 16) == BacnetPropertyReadStatus::Ack,
              "indexed priority array ACK") ||
      !expect(transport.lastPacket[17] == 0x29 && transport.lastPacket[18] == 0x10,
              "priority array index request")) {
    return false;
  }

  const uint8_t countValue[] = {0x21, 0x10};
  responseSize = buildReadAck(response, 3, objectId,
                              BacnetPropertyId::PriorityArray, 0,
                              countValue, sizeof(countValue));
  if (!expect(transport.queueResponse(response, responseSize), "queue priority array count ACK") ||
      !expect(object.readPriorityArray(value, 3, 0) == BacnetPropertyReadStatus::Ack,
              "priority array count ACK") ||
      !expect(value.type == BacnetValueType::Unsigned && value.unsignedValue == 16,
              "priority array count value") ||
      !expect(transport.lastPacket[17] == 0x29 && transport.lastPacket[18] == 0,
              "priority array count request")) {
    return false;
  }

  responseSize = buildReadAck(response, 4, objectId,
                              BacnetPropertyId::RelinquishDefault,
                              kBacnetNoArrayIndex,
                              realValue, sizeof(realValue));
  if (!expect(transport.queueResponse(response, responseSize), "queue relinquish default ACK") ||
      !expect(object.readRelinquishDefault(value, 3) == BacnetPropertyReadStatus::Ack,
              "relinquish default ACK")) {
    return false;
  }

  responseSize = buildReadError(response, 5, 2, 32);
  if (!expect(transport.queueResponse(response, responseSize), "queue unsupported property") ||
      !expect(object.readPriorityArray(priorityArray, 3) == BacnetPropertyReadStatus::UnsupportedProperty,
              "unsupported property status")) {
    return false;
  }

  responseSize = buildReadError(response, 6, 5, 1);
  if (!expect(transport.queueResponse(response, responseSize), "queue BACnet error") ||
      !expect(object.readPriorityArray(priorityArray, 3) == BacnetPropertyReadStatus::Error,
              "BACnet error status")) {
    return false;
  }

  responseSize = buildTerminalResponse(response, 0x60, 7);
  if (!expect(transport.queueResponse(response, responseSize), "queue reject") ||
      !expect(object.readPriorityArray(priorityArray, 3) == BacnetPropertyReadStatus::Reject,
              "reject status")) {
    return false;
  }

  responseSize = buildTerminalResponse(response, 0x70, 8);
  if (!expect(transport.queueResponse(response, responseSize), "queue abort") ||
      !expect(object.readPriorityArray(priorityArray, 3) == BacnetPropertyReadStatus::Abort,
              "abort status")) {
    return false;
  }

  if (!expect(object.readPriorityArray(priorityArray, 1) == BacnetPropertyReadStatus::Timeout,
              "timeout status")) {
    return false;
  }

  responseSize = buildPriorityArrayAck(response, 99, objectId, slotEight, slotSixteen);
  if (!expect(transport.queueResponse(response, responseSize), "queue foreign invoke")) {
    return false;
  }
  responseSize = buildPriorityArrayAck(response, 10, objectId, slotEight, slotSixteen);
  if (!expect(transport.queueResponse(response, responseSize),
              "queue matching invoke") ||
      !expect(object.readPriorityArray(priorityArray, 3) == BacnetPropertyReadStatus::Ack,
              "foreign invoke ignored")) {
    return false;
  }

  const BacnetPropertyRequest request{
    objectId, BacnetPropertyId::PriorityArray, kBacnetNoArrayIndex};
  responseSize = buildReadAck(response, 10, objectId, BacnetPropertyId::PriorityArray,
                              kBacnetNoArrayIndex, realValue, sizeof(realValue));
  if (!expect(!BacnetProtocol::parseReadPriorityArrayAck(
                response, responseSize, 10, request, priorityArray),
              "short priority array rejected")) {
    return false;
  }

  uint8_t nullValues[17] = {};
  responseSize = buildReadAck(response, 11, objectId, BacnetPropertyId::PriorityArray,
                              kBacnetNoArrayIndex, nullValues, sizeof(nullValues));
  if (!expect(!BacnetProtocol::parseReadPriorityArrayAck(
                response, responseSize, 11, request, priorityArray),
              "long priority array rejected")) {
    return false;
  }

  responseSize = buildPriorityArrayAck(response, 12, objectId, slotEight, slotSixteen);
  response[responseSize++] = 0;
  response[2] = static_cast<uint8_t>(responseSize >> 8);
  response[3] = static_cast<uint8_t>(responseSize);
  if (!expect(!BacnetProtocol::parseReadPriorityArrayAck(
                response, responseSize, 12, request, priorityArray),
              "priority array trailing data rejected")) {
    return false;
  }

  uint8_t malformedValues[17] = {0x45, 0xFF};
  responseSize = buildReadAck(response, 13, objectId, BacnetPropertyId::PriorityArray,
                              kBacnetNoArrayIndex, malformedValues, sizeof(malformedValues));
  if (!expect(!BacnetProtocol::parseReadPriorityArrayAck(
                response, responseSize, 13, request, priorityArray),
              "malformed priority array rejected")) {
    return false;
  }

  return true;
}

bool testWriteHelpers() {
  FakeTransport transport;
  FakeClock clock;
  transport.clock = &clock;
  BacnetClient client(transport, &clock);
  BacnetDeviceSession session(
    client, 1234, BacnetIpEndpoint(192, 0, 2, 1, BacnetClient::kDefaultPort));
  BacnetRemoteObject object = session.object(BacnetObjectType::AnalogValue, 7);
  if (!expect(client.begin(), "fake client begin")) {
    return false;
  }
  uint8_t response[64] = {};

  size_t responseSize = buildWriteAck(response, 1);
  if (!expect(transport.queueResponse(response, responseSize),
              "queue write ACK")) {
    return false;
  }
  BacnetValue value;
  value.type = BacnetValueType::Real;
  value.realValue = 1.0F;
  if (!expect(object.writePresentValue(value, 16, 3) == BacnetDeviceSessionWriteStatus::Ack,
              "priority write ACK") ||
      !expect(transport.lastPacket[24] == 0x49 && transport.lastPacket[25] == 16,
              "priority write encoding")) {
    return false;
  }

  responseSize = buildWriteAck(response, 2);
  if (!expect(transport.queueResponse(response, responseSize),
              "queue relinquish ACK") ||
      !expect(object.relinquishPresentValue(16, 3) == BacnetDeviceSessionWriteStatus::Ack,
              "relinquish ACK")) {
    return false;
  }
  const uint8_t expectedRelinquish[] = {
    0x81, 0x0A, 0x00, 0x16, 0x01, 0x04, 0x00, 0x05, 0x02, 0x0F,
    0x0C, 0x00, 0x80, 0x00, 0x07, 0x19, 0x55, 0x3E, 0x00, 0x3F,
    0x49, 0x10,
  };
  return expectBytes(transport.lastPacket, transport.lastPacketSize,
                     expectedRelinquish, sizeof(expectedRelinquish),
                     "relinquish null write encoding");
}

bool testRelinquishAllPriorities() {
  FakeTransport transport;
  FakeClock clock;
  transport.clock = &clock;
  BacnetClient client(transport, &clock);
  BacnetDeviceSession session(
    client, 1234, BacnetIpEndpoint(192, 0, 2, 1, BacnetClient::kDefaultPort));
  BacnetRemoteObject object = session.object(BacnetObjectType::AnalogValue, 7);
  if (!expect(client.begin(), "fake client begin")) {
    return false;
  }
  uint8_t response[64] = {};
  for (uint8_t priority = 1; priority <= 16; ++priority) {
    const size_t responseSize = buildWriteAck(response, priority);
    if (!expect(transport.queueResponse(response, responseSize),
                "queue priority reset ACK")) {
      return false;
    }
  }

  const BacnetPriorityRelinquishResult result = object.relinquishAllPriorities(3);
  if (!expect(result.succeeded(), "priority reset success") ||
      !expect(transport.sendCount == 16, "priority reset write count")) {
    return false;
  }
  for (uint8_t priority = 1; priority <= 16; ++priority) {
    const size_t packetIndex = priority - 1;
    if (!expect(transport.sentPacketSizes[packetIndex] == 22 &&
                  transport.sentPackets[packetIndex][18] == 0x00 &&
                  transport.sentPackets[packetIndex][20] == 0x49 &&
                  transport.sentPackets[packetIndex][21] == priority,
                "priority reset null write encoding")) {
      return false;
    }
  }

  FakeTransport failureTransport;
  FakeClock failureClock;
  failureTransport.clock = &failureClock;
  BacnetClient failureClient(failureTransport, &failureClock);
  BacnetDeviceSession failureSession(
    failureClient, 1234, BacnetIpEndpoint(192, 0, 2, 1, BacnetClient::kDefaultPort));
  BacnetRemoteObject failureObject =
    failureSession.object(BacnetObjectType::AnalogValue, 7);
  if (!expect(failureClient.begin(), "failure fake client begin")) {
    return false;
  }
  size_t responseSize = buildWriteAck(response, 1);
  if (!expect(failureTransport.queueResponse(response, responseSize),
              "queue first priority ACK")) {
    return false;
  }
  responseSize = buildTerminalResponse(response, 0x60, 2);
  if (!expect(failureTransport.queueResponse(response, responseSize),
              "queue second priority reject")) {
    return false;
  }
  const BacnetPriorityRelinquishResult failure =
    failureObject.relinquishAllPriorities(3);
  return expect(failure.status == BacnetDeviceSessionWriteStatus::Reject &&
                  failure.completedPriorities == 1 && failure.failedPriority == 2 &&
                  failureTransport.sendCount == 2,
                "priority reset stops at first failure");
}

} // namespace

int main() {
  return testReadHelpers() && testWriteHelpers() && testRelinquishAllPriorities()
           ? 0
           : 1;
}
