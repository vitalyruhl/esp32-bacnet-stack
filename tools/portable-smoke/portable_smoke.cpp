// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetServer.h"
#include "portable/BacnetDisplayText.h"

#include <cstring>

// ASHRAE-reserved vendor ID for tests and examples only; never a product ID.
constexpr uint16_t kTestVendorId = 555;

class SmokeTransport final : public BacnetDatagramTransport {
public:
  bool begin(uint16_t) override {
    return true;
  }
  void end() override {}
  bool send(const BacnetIpEndpoint& destination, const uint8_t* data, size_t length) override {
    if (data == nullptr || length > sizeof(lastSent)) {
      return false;
    }
    lastDestination = destination;
    lastSentLength = length;
    std::memcpy(lastSent, data, length);
    ++sendCalls;
    return true;
  }
  size_t receive(uint8_t* buffer, size_t capacity, BacnetIpEndpoint& source) override {
    if (incomingLength == 0 || incomingLength > capacity) {
      return 0;
    }
    std::memcpy(buffer, incoming, incomingLength);
    source = incomingSource;
    const size_t result = incomingLength;
    incomingLength = 0;
    return result;
  }
  void queue(const uint8_t* data, size_t length, const BacnetIpEndpoint& source) {
    if (data == nullptr || length > sizeof(incoming)) {
      incomingLength = 0;
      return;
    }
    std::memcpy(incoming, data, length);
    incomingLength = length;
    incomingSource = source;
  }
  void idle() override {}

  uint8_t incoming[BacnetServer::kMaxDatagramSize] = {};
  size_t incomingLength = 0;
  BacnetIpEndpoint incomingSource;
  uint8_t lastSent[BacnetServer::kMaxDatagramSize] = {};
  size_t lastSentLength = 0;
  BacnetIpEndpoint lastDestination;
  uint32_t sendCalls = 0;
};

int main() {
  uint8_t frame[BacnetProtocol::kWhoIsRequestSize] = {};
  const BacnetValue value{};
  SmokeTransport transport;
  BacnetServer server(transport);
  BacnetServerDevice device;
  device.deviceInstance = 1234;
  device.vendorId = kTestVendorId;
  device.maxApduLengthAccepted = 1024;
  device.objectName = "Test Device";
  device.vendorName = "Test Vendor";
  device.modelName = "Test Model";
  device.firmwareRevision = "1.0";
  device.applicationSoftwareVersion = "1.0";
  const BacnetIpEndpoint source(192, 0, 2, 44, 47809);

  const bool baseline =
    BacnetProtocol::buildWhoIsRequest(frame, sizeof(frame)) == sizeof(frame) &&
    bacnetObjectTypePrefix(BacnetObjectType::AnalogValue)[0] == 'A' &&
    !bacnetValueInRange(value, 0.0f, 1.0f) && server.begin(device);
  if (!baseline) {
    return 1;
  }

  const uint8_t truncatedConfirmed[] = {
    0x81,
    0x0A,
    0x00,
    0x09,
    0x01,
    0x04,
    0x00,
    0x05,
    0x42,
  };
  transport.queue(truncatedConfirmed, sizeof(truncatedConfirmed), source);
  const bool truncation = server.poll() == BacnetServerPollResult::Malformed &&
                          transport.sendCalls == 0;

  transport.queue(frame, sizeof(frame), source);
  BacnetIAmDeviceInfo iam;
  const bool whoIs =
    server.poll() == BacnetServerPollResult::IAmSent &&
    transport.sendCalls == 1 && transport.lastDestination.port == source.port &&
    BacnetProtocol::parseIAmResponse(transport.lastSent, transport.lastSentLength, iam) &&
    iam.deviceInstance == device.deviceInstance && iam.vendorId == device.vendorId;

  const uint8_t includedRange[] = {
    0x81,
    0x0B,
    0x00,
    0x0E,
    0x01,
    0x00,
    0x10,
    0x08,
    0x0A,
    0x04,
    0xB0,
    0x1A,
    0x04,
    0xD2,
  };
  transport.queue(includedRange, sizeof(includedRange), source);
  const bool included = server.poll() == BacnetServerPollResult::IAmSent &&
                        transport.sendCalls == 2;

  const uint8_t excludedRange[] = {
    0x81,
    0x0B,
    0x00,
    0x0E,
    0x01,
    0x00,
    0x10,
    0x08,
    0x0A,
    0x04,
    0xD3,
    0x1A,
    0x04,
    0xD4,
  };
  transport.queue(excludedRange, sizeof(excludedRange), source);
  const bool excluded = server.poll() == BacnetServerPollResult::Ignored &&
                        transport.sendCalls == 2;

  const uint8_t incompleteRange[] = {
    0x81,
    0x0B,
    0x00,
    0x0A,
    0x01,
    0x00,
    0x10,
    0x08,
    0x09,
    0x2A,
  };
  transport.queue(incompleteRange, sizeof(incompleteRange), source);
  const bool incomplete = server.poll() == BacnetServerPollResult::Malformed &&
                          transport.sendCalls == 2;

  const uint8_t invalidRange[] = {
    0x81,
    0x0B,
    0x00,
    0x10,
    0x01,
    0x00,
    0x10,
    0x08,
    0x0B,
    0x40,
    0x00,
    0x00,
    0x1B,
    0x40,
    0x00,
    0x01,
  };
  transport.queue(invalidRange, sizeof(invalidRange), source);
  const bool invalid = server.poll() == BacnetServerPollResult::Malformed &&
                       transport.sendCalls == 2;

  const uint8_t confirmedReadProperty[] = {
    0x81,
    0x0A,
    0x00,
    0x0A,
    0x01,
    0x04,
    0x00,
    0x05,
    0x42,
    0x0D,
  };
  transport.queue(confirmedReadProperty, sizeof(confirmedReadProperty), source);
  const bool reject =
    server.poll() == BacnetServerPollResult::RejectSent &&
    transport.sendCalls == 3 && transport.lastSentLength == BacnetProtocol::kRejectResponseSize &&
    transport.lastSent[6] == 0x60 && transport.lastSent[7] == 0x42 &&
    transport.lastSent[8] == BacnetServer::kRejectReasonUnrecognizedService;

  const auto readProperty = [&](BacnetObjectId object, BacnetPropertyId property, uint32_t arrayIndex, uint8_t invokeId) {
    uint8_t request[BacnetProtocol::kMaxReadPropertyRequestSize] = {};
    const BacnetPropertyRequest read{object, property, arrayIndex};
    const size_t requestSize = BacnetProtocol::buildReadPropertyRequest(
      request, sizeof(request), read, invokeId);
    if (requestSize == 0)
      return false;
    transport.queue(request, requestSize, source);
    return server.poll() == BacnetServerPollResult::ReadPropertyAckSent;
  };
  const BacnetObjectId deviceObject{
    static_cast<uint16_t>(BacnetObjectType::Device), device.deviceInstance};
  const BacnetPropertyId scalarProperties[] = {
    BacnetPropertyId::ObjectIdentifier,
    BacnetPropertyId::ObjectName,
    BacnetPropertyId::ObjectType,
    BacnetPropertyId::SystemStatus,
    BacnetPropertyId::VendorName,
    BacnetPropertyId::VendorIdentifier,
    BacnetPropertyId::ModelName,
    BacnetPropertyId::FirmwareRevision,
    BacnetPropertyId::ApplicationSoftwareVersion,
    BacnetPropertyId::ProtocolVersion,
    BacnetPropertyId::ProtocolRevision,
    BacnetPropertyId::ProtocolServicesSupported,
    BacnetPropertyId::ProtocolObjectTypesSupported,
    BacnetPropertyId::MaxApduLengthAccepted,
    BacnetPropertyId::SegmentationSupported,
    BacnetPropertyId::ApduTimeout,
    BacnetPropertyId::NumberOfApduRetries,
    BacnetPropertyId::DatabaseRevision,
  };
  bool scalars = true;
  uint8_t scalarInvoke = 10;
  for (const auto property : scalarProperties) {
    BacnetValue parsed;
    scalars = scalars && readProperty(deviceObject, property, kBacnetNoArrayIndex, scalarInvoke) &&
              BacnetProtocol::parseReadPropertyAck(
                transport.lastSent, transport.lastSentLength, scalarInvoke, BacnetPropertyRequest{deviceObject, property, kBacnetNoArrayIndex}, parsed);
    ++scalarInvoke;
  }

  BacnetValue objectList;
  const bool objectListFull =
    readProperty(deviceObject, BacnetPropertyId::ObjectList, kBacnetNoArrayIndex, 40) &&
    BacnetProtocol::parseReadPropertyAck(transport.lastSent, transport.lastSentLength, 40, BacnetPropertyRequest{deviceObject, BacnetPropertyId::ObjectList, kBacnetNoArrayIndex}, objectList) &&
    objectList.type == BacnetValueType::ObjectIdentifierList;
  BacnetValue objectCount;
  const bool objectListCount =
    readProperty(deviceObject, BacnetPropertyId::ObjectList, 0, 41) &&
    BacnetProtocol::parseReadPropertyAck(transport.lastSent, transport.lastSentLength, 41, BacnetPropertyRequest{deviceObject, BacnetPropertyId::ObjectList, 0}, objectCount) &&
    objectCount.unsignedValue == 1;
  BacnetValue objectEntry;
  const bool objectListEntry =
    readProperty(deviceObject, BacnetPropertyId::ObjectList, 1, 42) &&
    BacnetProtocol::parseReadPropertyAck(transport.lastSent, transport.lastSentLength, 42, BacnetPropertyRequest{deviceObject, BacnetPropertyId::ObjectList, 1}, objectEntry) &&
    objectEntry.objectValue.instance == device.deviceInstance;

  uint8_t request[BacnetProtocol::kMaxReadPropertyRequestSize] = {};
  const auto expectError = [&](BacnetPropertyRequest read, uint8_t invokeId, uint32_t expectedCode) {
    const size_t requestSize = BacnetProtocol::buildReadPropertyRequest(
      request, sizeof(request), read, invokeId);
    if (requestSize == 0)
      return false;
    transport.queue(request, requestSize, source);
    BacnetValue error;
    uint32_t errorClass = 0;
    uint32_t errorCode = 0;
    return server.poll() == BacnetServerPollResult::ReadPropertyErrorSent &&
           BacnetProtocol::parseReadPropertyError(transport.lastSent,
                                                  transport.lastSentLength,
                                                  invokeId,
                                                  error,
                                                  &errorClass,
                                                  &errorCode) &&
           errorCode == expectedCode;
  };
  const bool errors =
    expectError(BacnetPropertyRequest{deviceObject, BacnetPropertyId::ObjectList, 2}, 43, 42) &&
    expectError(BacnetPropertyRequest{deviceObject, BacnetPropertyId::ObjectName, 0}, 44, 42) &&
    expectError(BacnetPropertyRequest{BacnetObjectId{8, 7}, BacnetPropertyId::ObjectName, kBacnetNoArrayIndex}, 45, 31) &&
    expectError(BacnetPropertyRequest{deviceObject, static_cast<BacnetPropertyId>(999), kBacnetNoArrayIndex}, 46, 32);
  const bool propertyList =
    readProperty(deviceObject, BacnetPropertyId::PropertyList, 0, 47) &&
    transport.lastSentLength > 16 && transport.lastSent[transport.lastSentLength - 1] == 0x3F &&
    readProperty(deviceObject, BacnetPropertyId::PropertyList, 1, 48) &&
    expectError(BacnetPropertyRequest{deviceObject, BacnetPropertyId::PropertyList, 21}, 49, 42);
  const bool addressBinding =
    readProperty(deviceObject, BacnetPropertyId::DeviceAddressBinding, kBacnetNoArrayIndex, 50) &&
    transport.lastSent[transport.lastSentLength - 2] == 0x3E &&
    transport.lastSent[transport.lastSentLength - 1] == 0x3F &&
    expectError(BacnetPropertyRequest{deviceObject, BacnetPropertyId::DeviceAddressBinding, 0}, 51, 42);
  const size_t malformedSize = BacnetProtocol::buildReadPropertyRequest(
    request, sizeof(request), BacnetPropertyRequest{deviceObject, BacnetPropertyId::ObjectName, kBacnetNoArrayIndex}, 52);
  const uint32_t sendsBeforeMalformed = transport.sendCalls;
  transport.queue(request, malformedSize - 1, source);
  const bool malformedRead = server.poll() == BacnetServerPollResult::Malformed &&
                             transport.sendCalls == sendsBeforeMalformed;

  return whoIs && included && excluded && truncation && incomplete && invalid && reject &&
             scalars && objectListFull && objectListCount && objectListEntry && errors &&
             propertyList && addressBinding && malformedRead
           ? 0
           : 1;
}
