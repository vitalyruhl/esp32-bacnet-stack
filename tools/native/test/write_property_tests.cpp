// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetClient.h"

#include <cstdio>
#include <cstring>

namespace {
bool expect(bool condition, const char* text) {
  if (!condition) {
    std::fprintf(stderr, "[E] %s\n", text);
  }
  return condition;
}

bool expectBytes(const uint8_t* actual, size_t actualSize,
                 const uint8_t* expected, size_t expectedSize,
                 const char* text) {
  return expect(actualSize == expectedSize &&
                  std::memcmp(actual, expected, expectedSize) == 0,
                text);
}

class FakeTransport final : public BacnetDatagramTransport {
public:
  bool begin(uint16_t) override { return beginResult; }
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
    if (responseSize == 0 || capacity < responseSize) {
      return 0;
    }
    std::memcpy(data, response, responseSize);
    const size_t size = responseSize;
    responseSize = 0;
    return size;
  }
  void idle() override {}

  bool beginResult = true;
  bool sendResult = true;
  unsigned sendCount = 0;
  uint8_t lastPacket[600] = {};
  size_t lastPacketSize = 0;
  uint8_t response[32] = {};
  size_t responseSize = 0;
};

bool testApplicationValues() {
  uint8_t bytes[600] = {};
  BacnetValue value;
  const uint8_t nullValue[] = {0x00};
  value.type = BacnetValueType::Null;
  if (!expectBytes(bytes, BacnetProtocol::encodeApplicationValue(bytes, sizeof(bytes), value), nullValue, sizeof(nullValue), "null encoding")) return false;
  const uint8_t booleanValue[] = {0x11};
  value = BacnetValue{}; value.type = BacnetValueType::Boolean; value.booleanValue = true;
  if (!expectBytes(bytes, BacnetProtocol::encodeApplicationValue(bytes, sizeof(bytes), value), booleanValue, sizeof(booleanValue), "boolean encoding")) return false;
  const uint8_t unsignedValue[] = {0x22, 0x12, 0x34};
  value = BacnetValue{}; value.type = BacnetValueType::Unsigned; value.unsignedValue = 0x1234;
  if (!expectBytes(bytes, BacnetProtocol::encodeApplicationValue(bytes, sizeof(bytes), value), unsignedValue, sizeof(unsignedValue), "unsigned encoding")) return false;
  const uint8_t signedValue[] = {0x31, 0xFE};
  value = BacnetValue{}; value.type = BacnetValueType::Signed; value.signedValue = -2;
  if (!expectBytes(bytes, BacnetProtocol::encodeApplicationValue(bytes, sizeof(bytes), value), signedValue, sizeof(signedValue), "signed encoding")) return false;
  const uint8_t realValue[] = {0x44, 0x3F, 0x80, 0x00, 0x00};
  value = BacnetValue{}; value.type = BacnetValueType::Real; value.realValue = 1.0F;
  if (!expectBytes(bytes, BacnetProtocol::encodeApplicationValue(bytes, sizeof(bytes), value), realValue, sizeof(realValue), "real encoding")) return false;
  const uint8_t enumValue[] = {0x91, 0x05};
  value = BacnetValue{}; value.type = BacnetValueType::Enumerated; value.unsignedValue = 5;
  if (!expectBytes(bytes, BacnetProtocol::encodeApplicationValue(bytes, sizeof(bytes), value), enumValue, sizeof(enumValue), "enumerated encoding")) return false;
  const uint8_t stringValue[] = {0x73, 0x00, 'A', 'B'};
  value = BacnetValue{}; value.type = BacnetValueType::CharacterString; value.textLength = 2; std::memcpy(value.text, "AB", 2);
  if (!expectBytes(bytes, BacnetProtocol::encodeApplicationValue(bytes, sizeof(bytes), value), stringValue, sizeof(stringValue), "character string encoding")) return false;
  const uint8_t objectValue[] = {0xC4, 0x00, 0x00, 0x00, 0x2A};
  value = BacnetValue{}; value.type = BacnetValueType::ObjectIdentifier; value.objectValue = BacnetObjectId{0, 42};
  return expectBytes(bytes, BacnetProtocol::encodeApplicationValue(bytes, sizeof(bytes), value), objectValue, sizeof(objectValue), "object identifier encoding");
}

bool testRequestAndResponses() {
  FakeTransport transport;
  BacnetClient client(transport);
  if (!expect(client.begin(), "fake client begin")) return false;
  BacnetValue value; value.type = BacnetValueType::Real; value.realValue = 1.0F;
  const BacnetPropertyRequest request{BacnetObjectId{2, 7}, BacnetPropertyId::PresentValue};
  if (!expect(client.sendWriteProperty(BacnetIpEndpoint(192, 0, 2, 1, 47808), request, value, 9) == BacnetWritePropertyPollStatus::None, "write send")) return false;
  const uint8_t expected[] = {0x81,0x0A,0x00,0x18,0x01,0x04,0x00,0x05,0x09,0x0F,0x0C,0x00,0x80,0x00,0x07,0x19,0x55,0x3E,0x44,0x3F,0x80,0x00,0x00,0x3F};
  if (!expectBytes(transport.lastPacket, transport.lastPacketSize, expected, sizeof(expected), "write request encoding")) return false;
  BacnetWritePropertyOptions priorityOptions;
  priorityOptions.hasPriority = true;
  priorityOptions.priority = 16;
  const uint8_t priorityExpected[] = {0x81,0x0A,0x00,0x1A,0x01,0x04,0x00,0x05,0x09,0x0F,0x0C,0x00,0x80,0x00,0x07,0x19,0x55,0x3E,0x44,0x3F,0x80,0x00,0x00,0x3F,0x49,0x10};
  uint8_t priorityPacket[64] = {};
  if (!expectBytes(priorityPacket, BacnetProtocol::buildWritePropertyRequest(priorityPacket, sizeof(priorityPacket), request.object, request.property, value, priorityOptions, 9), priorityExpected, sizeof(priorityExpected), "priority 16 encoding")) return false;
  priorityOptions.priority = 1;
  if (!expect(BacnetProtocol::buildWritePropertyRequest(priorityPacket, sizeof(priorityPacket), request.object, request.property, value, priorityOptions, 9) == sizeof(priorityExpected) && priorityPacket[25] == 1, "priority 1 encoding")) return false;
  priorityOptions.priority = 8;
  if (!expect(BacnetProtocol::buildWritePropertyRequest(priorityPacket, sizeof(priorityPacket), request.object, request.property, value, priorityOptions, 9) == sizeof(priorityExpected) && priorityPacket[25] == 8, "priority 8 encoding")) return false;
  const BacnetPropertyRequest indexedRequest{BacnetObjectId{2, 7}, BacnetPropertyId::PresentValue, 3};
  uint8_t indexed[64] = {};
  const size_t indexedSize = BacnetProtocol::buildWritePropertyRequest(indexed, sizeof(indexed), indexedRequest, value, 9);
  if (!expect(indexedSize != 0 && indexed[17] == 0x29 && indexed[18] == 0x03 && indexed[19] == 0x3E, "write array index encoding")) return false;
  priorityOptions.arrayIndex = 3;
  priorityOptions.priority = 16;
  if (!expect(BacnetProtocol::buildWritePropertyRequest(indexed, sizeof(indexed), indexedRequest.object, indexedRequest.property, value, priorityOptions, 9) == indexedSize + 2 && indexed[indexedSize] == 0x49 && indexed[indexedSize + 1] == 0x10, "array index plus priority")) return false;
  BacnetValue relinquishDefaultValue;
  relinquishDefaultValue.type = BacnetValueType::Enumerated;
  relinquishDefaultValue.unsignedValue = 1;
  uint8_t relinquishDefaultPacket[64] = {};
  const BacnetPropertyRequest relinquishDefaultRequest{
    request.object, BacnetPropertyId::RelinquishDefault};
  const size_t relinquishDefaultSize = BacnetProtocol::buildWritePropertyRequest(
    relinquishDefaultPacket, sizeof(relinquishDefaultPacket),
    relinquishDefaultRequest, relinquishDefaultValue, 9);
  BacnetWritePropertyOptions internalNoPriority;
  internalNoPriority.hasPriority = true;
  internalNoPriority.priority = 0;
  uint8_t internalNoPriorityPacket[64] = {};
  if (!expectBytes(
        internalNoPriorityPacket,
        BacnetProtocol::buildWritePropertyRequest(
          internalNoPriorityPacket, sizeof(internalNoPriorityPacket), request.object,
          BacnetPropertyId::RelinquishDefault, relinquishDefaultValue,
          internalNoPriority, 9),
        relinquishDefaultPacket, relinquishDefaultSize,
        "priority zero omits tag for relinquish default")) return false;
  if (!expect(BacnetProtocol::buildWritePropertyRequest(
                priorityPacket, sizeof(priorityPacket), request.object,
                BacnetPropertyId::PresentValue, value, internalNoPriority, 9) == 0,
              "priority zero rejected for present value")) return false;
  internalNoPriority.priority = 17;
  if (!expect(BacnetProtocol::buildWritePropertyRequest(
                priorityPacket, sizeof(priorityPacket), request.object,
                BacnetPropertyId::RelinquishDefault, relinquishDefaultValue,
                internalNoPriority, 9) == 0,
              "priority above 16 rejected")) return false;
  priorityOptions.arrayIndex = kBacnetNoArrayIndex;
  priorityOptions.priority = 0;
  if (!expect(client.sendWriteProperty(BacnetIpEndpoint(192, 0, 2, 1, 47808), request.object, request.property, value, priorityOptions, 10) == BacnetWritePropertyPollStatus::InvalidArgument && transport.sendCount == 1, "client priority zero rejects without send")) return false;
  priorityOptions.priority = 17;
  if (!expect(client.sendWriteProperty(BacnetIpEndpoint(192, 0, 2, 1, 47808), request.object, request.property, value, priorityOptions, 11) == BacnetWritePropertyPollStatus::InvalidArgument && transport.sendCount == 1, "client priority above 16 rejects without send")) return false;
  BacnetValue nullValue;
  nullValue.type = BacnetValueType::Null;
  priorityOptions.priority = 8;
  const size_t nullPrioritySize = BacnetProtocol::buildWritePropertyRequest(
    priorityPacket, sizeof(priorityPacket), request.object,
    BacnetPropertyId::PresentValue, nullValue, priorityOptions, 12);
  BacnetWritePropertyRequestHeader nullPriorityRequest;
  if (!expect(nullPrioritySize != 0 &&
              BacnetProtocol::parseWritePropertyRequest(
                priorityPacket, nullPrioritySize, nullPriorityRequest) ==
                  BacnetWritePropertyRequestParseStatus::WriteProperty &&
              nullPriorityRequest.hasPriority &&
              nullPriorityRequest.priority == 8 &&
              nullPriorityRequest.value.type == BacnetValueType::Null,
              "null relinquish with priority 8")) return false;
  const uint8_t ack[] = {0x81,0x0A,0x00,0x0A,0x01,0x00,0x20,0x09,0x0F,0x0F};
  std::memcpy(transport.response, ack, sizeof(ack)); transport.responseSize = sizeof(ack);
  if (!expect(client.pollWriteProperty(9) == BacnetWritePropertyPollStatus::Ack, "write ack")) return false;
  std::memcpy(transport.response, ack, sizeof(ack)); transport.response[7] = 8; transport.responseSize = sizeof(ack);
  if (!expect(client.pollWriteProperty(9) == BacnetWritePropertyPollStatus::None, "foreign invoke ignored")) return false;
  const uint8_t error[] = {0x81,0x0A,0x00,0x0A,0x01,0x00,0x50,0x09,0x0F,0x0F};
  std::memcpy(transport.response, error, sizeof(error)); transport.responseSize = sizeof(error);
  if (!expect(client.pollWriteProperty(9) == BacnetWritePropertyPollStatus::Error, "write error")) return false;
  const uint8_t notCommandable[] = {0x81,0x0A,0x00,0x0D,0x01,0x00,0x50,0x09,0x0F,0x91,0x02,0x91,40};
  std::memcpy(transport.response, notCommandable, sizeof(notCommandable)); transport.responseSize = sizeof(notCommandable);
  if (!expect(client.pollWriteProperty(9) == BacnetWritePropertyPollStatus::NotCommandable, "write access denied")) return false;
  const uint8_t reject[] = {0x81,0x0A,0x00,0x08,0x01,0x00,0x60,0x09};
  std::memcpy(transport.response, reject, sizeof(reject)); transport.responseSize = sizeof(reject);
  if (!expect(client.pollWriteProperty(9) == BacnetWritePropertyPollStatus::Reject, "write reject")) return false;
  const uint8_t abort[] = {0x81,0x0A,0x00,0x08,0x01,0x00,0x70,0x09};
  std::memcpy(transport.response, abort, sizeof(abort)); transport.responseSize = sizeof(abort);
  value.type = BacnetValueType::Unsupported;
  if (!expect(client.sendWriteProperty(BacnetIpEndpoint(192, 0, 2, 1, 47808), request, value, 9) == BacnetWritePropertyPollStatus::UnsupportedValue && transport.sendCount == 1, "unsupported value rejected without send")) return false;
  return expect(client.pollWriteProperty(9) == BacnetWritePropertyPollStatus::Abort, "write abort") &&
    expect(client.pollWriteProperty(9) == BacnetWritePropertyPollStatus::None, "write timeout poll");
}
}  // namespace

int main() {
  return testApplicationValues() && testRequestAndResponses() ? 0 : 1;
}
