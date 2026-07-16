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

  static constexpr size_t kMaxResponses = 3;
  FakeClock* clock = nullptr;
  bool sendResult = true;
  unsigned sendCount = 0;
  uint8_t lastPacket[600] = {};
  size_t lastPacketSize = 0;
  uint8_t responses[kMaxResponses][64] = {};
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
  uint8_t response[64] = {};
  BacnetValue value;

  size_t responseSize = buildReadAck(response, 1, objectId,
                                     BacnetPropertyId::PriorityArray,
                                     kBacnetNoArrayIndex,
                                     realValue, sizeof(realValue));
  if (!expect(transport.queueResponse(response, responseSize), "queue priority array ACK") ||
      !expect(object.readPriorityArray(value, 3) == BacnetPropertyReadStatus::Ack,
              "priority array ACK") ||
      !expect(value.type == BacnetValueType::Real && value.realValue == 1.0F,
              "priority array ACK value")) {
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

  responseSize = buildReadAck(response, 3, objectId,
                              BacnetPropertyId::RelinquishDefault,
                              kBacnetNoArrayIndex,
                              realValue, sizeof(realValue));
  if (!expect(transport.queueResponse(response, responseSize), "queue relinquish default ACK") ||
      !expect(object.readRelinquishDefault(value, 3) == BacnetPropertyReadStatus::Ack,
              "relinquish default ACK")) {
    return false;
  }

  responseSize = buildReadError(response, 4, 2, 32);
  if (!expect(transport.queueResponse(response, responseSize), "queue unsupported property") ||
      !expect(object.readPriorityArray(value, 3) == BacnetPropertyReadStatus::UnsupportedProperty,
              "unsupported property status")) {
    return false;
  }

  responseSize = buildReadError(response, 5, 5, 1);
  if (!expect(transport.queueResponse(response, responseSize), "queue BACnet error") ||
      !expect(object.readPriorityArray(value, 3) == BacnetPropertyReadStatus::Error,
              "BACnet error status")) {
    return false;
  }

  responseSize = buildTerminalResponse(response, 0x60, 6);
  if (!expect(transport.queueResponse(response, responseSize), "queue reject") ||
      !expect(object.readPriorityArray(value, 3) == BacnetPropertyReadStatus::Reject,
              "reject status")) {
    return false;
  }

  responseSize = buildTerminalResponse(response, 0x70, 7);
  if (!expect(transport.queueResponse(response, responseSize), "queue abort") ||
      !expect(object.readPriorityArray(value, 3) == BacnetPropertyReadStatus::Abort,
              "abort status")) {
    return false;
  }

  if (!expect(object.readPriorityArray(value, 1) == BacnetPropertyReadStatus::Timeout,
              "timeout status")) {
    return false;
  }

  responseSize = buildReadAck(response, 99, objectId,
                              BacnetPropertyId::PriorityArray,
                              kBacnetNoArrayIndex,
                              realValue, sizeof(realValue));
  if (!expect(transport.queueResponse(response, responseSize), "queue foreign invoke")) {
    return false;
  }
  responseSize = buildReadAck(response, 9, objectId,
                              BacnetPropertyId::PriorityArray,
                              kBacnetNoArrayIndex,
                              realValue, sizeof(realValue));
  if (!expect(transport.queueResponse(response, responseSize),
              "queue matching invoke") ||
      !expect(object.readPriorityArray(value, 3) == BacnetPropertyReadStatus::Ack,
              "foreign invoke ignored")) {
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

} // namespace

int main() {
  return testReadHelpers() && testWriteHelpers() ? 0 : 1;
}
