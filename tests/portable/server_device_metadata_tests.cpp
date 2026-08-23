// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetServer.h"

#include <cstring>

namespace {

class TestTransport final : public BacnetDatagramTransport {
public:
  bool begin(uint16_t) override {
    ++beginCalls;
    return true;
  }

  void end() override {}

  bool send(const BacnetIpEndpoint&, const uint8_t* data, size_t length) override {
    if (data == nullptr || length > sizeof(lastSent)) {
      return false;
    }
    std::memcpy(lastSent, data, length);
    lastSentLength = length;
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
  uint32_t beginCalls = 0;
};

constexpr BacnetServerDevice kDevice{
  5001,
  555,
  "Metadata Test Device",
  "Test Vendor",
  "Test Model",
  "1.0.0",
};

constexpr BacnetPropertyId kBaseProperties[] = {
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
  BacnetPropertyId::ObjectList,
  BacnetPropertyId::MaxApduLengthAccepted,
  BacnetPropertyId::SegmentationSupported,
  BacnetPropertyId::ApduTimeout,
  BacnetPropertyId::NumberOfApduRetries,
  BacnetPropertyId::DeviceAddressBinding,
  BacnetPropertyId::DatabaseRevision,
  BacnetPropertyId::PropertyList,
};

bool readTextProperty(const void* context, BacnetValue& value) {
  const auto* text = static_cast<const char*>(context);
  if (text == nullptr || std::strlen(text) >= sizeof(value.text)) {
    return false;
  }
  value = BacnetValue{};
  value.type = BacnetValueType::CharacterString;
  value.textLength = std::strlen(text);
  std::memcpy(value.text, text, value.textLength + 1U);
  return true;
}

bool readProperty(TestTransport& transport,
                  BacnetServer& server,
                  BacnetPropertyRequest request,
                  uint8_t invokeId,
                  BacnetValue& value) {
  uint8_t frame[BacnetProtocol::kMaxReadPropertyRequestSize] = {};
  const size_t frameSize = BacnetProtocol::buildReadPropertyRequest(
    frame, sizeof(frame), request, invokeId);
  if (frameSize == 0) {
    return false;
  }
  transport.queue(frame, frameSize, BacnetIpEndpoint(192, 0, 2, 44, 47808));
  return server.poll() == BacnetServerPollResult::ReadPropertyAckSent &&
         BacnetProtocol::parseReadPropertyAck(
           transport.lastSent, transport.lastSentLength, invokeId, request, value);
}

bool readPropertyError(TestTransport& transport,
                       BacnetServer& server,
                       BacnetPropertyRequest request,
                       uint8_t invokeId,
                       uint32_t expectedCode) {
  uint8_t frame[BacnetProtocol::kMaxReadPropertyRequestSize] = {};
  const size_t frameSize = BacnetProtocol::buildReadPropertyRequest(
    frame, sizeof(frame), request, invokeId);
  if (frameSize == 0) {
    return false;
  }
  transport.queue(frame, frameSize, BacnetIpEndpoint(192, 0, 2, 44, 47808));
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
         errorClass == 2 && errorCode == expectedCode;
}

bool propertyListMatches(TestTransport& transport,
                         BacnetServer& server,
                         const BacnetPropertyId* expected,
                         size_t expectedCount) {
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::Device),
                              kDevice.deviceInstance};
  BacnetValue value;
  if (!readProperty(transport,
                    server,
                    {object, BacnetPropertyId::PropertyList, 0},
                    1,
                    value) ||
      value.type != BacnetValueType::Unsigned || value.unsignedValue != expectedCount) {
    return false;
  }
  for (size_t index = 0; index < expectedCount; ++index) {
    if (!readProperty(transport,
                      server,
                      {object, BacnetPropertyId::PropertyList,
                       static_cast<uint32_t>(index + 1U)},
                      static_cast<uint8_t>(index + 2U),
                      value) ||
        value.type != BacnetValueType::Enumerated ||
        value.unsignedValue != static_cast<uint32_t>(expected[index])) {
      return false;
    }
  }
  return true;
}

bool testMinimalDeviceProfile() {
  TestTransport transport;
  BacnetServer server(transport);
  if (!server.begin(kDevice) || server.propertyRegistrationCount() != 0 ||
      !propertyListMatches(transport,
                           server,
                           kBaseProperties,
                           sizeof(kBaseProperties) / sizeof(kBaseProperties[0]))) {
    return false;
  }

  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::Device),
                              kDevice.deviceInstance};
  return readPropertyError(transport,
                           server,
                           {object, BacnetPropertyId::Description, kBacnetNoArrayIndex},
                           50,
                           32) &&
         readPropertyError(transport,
                           server,
                           {object, BacnetPropertyId::Location, kBacnetNoArrayIndex},
                           51,
                           32) &&
         readPropertyError(transport,
                           server,
                           {object, BacnetPropertyId::SerialNumber, kBacnetNoArrayIndex},
                           52,
                           32);
}

bool testDeviceMetadataProfile(const BacnetServerPropertyRegistration* registrations,
                               size_t registrationCount,
                               const BacnetPropertyId* optionalProperties,
                               const char* const* expectedValues,
                               size_t optionalCount) {
  TestTransport transport;
  BacnetServer server(transport);
  if (!server.setPropertyRegistrations(registrations, registrationCount) ||
      !server.begin(kDevice) || server.propertyRegistrationCount() != registrationCount) {
    return false;
  }

  BacnetPropertyId expected[sizeof(kBaseProperties) / sizeof(kBaseProperties[0]) + 3] = {};
  std::memcpy(expected, kBaseProperties, sizeof(kBaseProperties));
  for (size_t index = 0; index < optionalCount; ++index) {
    expected[sizeof(kBaseProperties) / sizeof(kBaseProperties[0]) + index] =
      optionalProperties[index];
  }
  if (!propertyListMatches(transport,
                           server,
                           expected,
                           sizeof(kBaseProperties) / sizeof(kBaseProperties[0]) + optionalCount)) {
    return false;
  }

  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::Device),
                              kDevice.deviceInstance};
  BacnetValue value;
  for (size_t index = 0; index < optionalCount; ++index) {
    if (!readProperty(transport,
                      server,
                      {object, optionalProperties[index], kBacnetNoArrayIndex},
                      static_cast<uint8_t>(60U + index),
                      value) ||
        value.type != BacnetValueType::CharacterString ||
        std::strcmp(value.text, expectedValues[index]) != 0) {
      return false;
    }
  }

  const BacnetPropertyId allMetadataProperties[] = {
    BacnetPropertyId::Description,
    BacnetPropertyId::Location,
    BacnetPropertyId::SerialNumber,
  };
  for (size_t metadataIndex = 0;
       metadataIndex < sizeof(allMetadataProperties) / sizeof(allMetadataProperties[0]);
       ++metadataIndex) {
    bool registered = false;
    for (size_t optionalIndex = 0; optionalIndex < optionalCount; ++optionalIndex) {
      if (optionalProperties[optionalIndex] == allMetadataProperties[metadataIndex]) {
        registered = true;
        break;
      }
    }
    if (!registered &&
        !readPropertyError(transport,
                           server,
                           {object, allMetadataProperties[metadataIndex], kBacnetNoArrayIndex},
                           static_cast<uint8_t>(70U + metadataIndex),
                           32)) {
      return false;
    }
  }
  return true;
}

bool testIndividualMetadataOptIns() {
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::Device),
                              kDevice.deviceInstance};
  static constexpr char kDescription[] = "Mechanical room controller";
  static constexpr char kLocation[] = "Building A / Floor 2";
  static constexpr char kSerialNumber[] = "SN-000042";
  const BacnetServerPropertyRegistration description[] = {
    {object, BacnetPropertyId::Description, readTextProperty, kDescription},
  };
  const BacnetServerPropertyRegistration location[] = {
    {object, BacnetPropertyId::Location, readTextProperty, kLocation},
  };
  const BacnetServerPropertyRegistration serialNumber[] = {
    {object, BacnetPropertyId::SerialNumber, readTextProperty, kSerialNumber},
  };
  const BacnetServerPropertyRegistration descriptionAndSerial[] = {
    {object, BacnetPropertyId::Description, readTextProperty, kDescription},
    {object, BacnetPropertyId::SerialNumber, readTextProperty, kSerialNumber},
  };
  const BacnetServerPropertyRegistration allMetadata[] = {
    {object, BacnetPropertyId::Description, readTextProperty, kDescription},
    {object, BacnetPropertyId::Location, readTextProperty, kLocation},
    {object, BacnetPropertyId::SerialNumber, readTextProperty, kSerialNumber},
  };
  const BacnetPropertyId descriptionProperty[] = {BacnetPropertyId::Description};
  const BacnetPropertyId locationProperty[] = {BacnetPropertyId::Location};
  const BacnetPropertyId serialProperty[] = {BacnetPropertyId::SerialNumber};
  const BacnetPropertyId descriptionAndSerialProperties[] = {
    BacnetPropertyId::Description,
    BacnetPropertyId::SerialNumber,
  };
  const BacnetPropertyId allProperties[] = {
    BacnetPropertyId::Description,
    BacnetPropertyId::Location,
    BacnetPropertyId::SerialNumber,
  };
  const char* const descriptionValue[] = {kDescription};
  const char* const locationValue[] = {kLocation};
  const char* const serialValue[] = {kSerialNumber};
  const char* const descriptionAndSerialValues[] = {kDescription, kSerialNumber};
  const char* const allValues[] = {kDescription, kLocation, kSerialNumber};

  return testDeviceMetadataProfile(description, 1, descriptionProperty, descriptionValue, 1) &&
         testDeviceMetadataProfile(location, 1, locationProperty, locationValue, 1) &&
         testDeviceMetadataProfile(serialNumber, 1, serialProperty, serialValue, 1) &&
         testDeviceMetadataProfile(descriptionAndSerial,
                                   2,
                                   descriptionAndSerialProperties,
                                   descriptionAndSerialValues,
                                   2) &&
         testDeviceMetadataProfile(allMetadata, 3, allProperties, allValues, 3);
}

bool testDeviceMetadataValidation() {
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::Device),
                              kDevice.deviceInstance};
  const BacnetServerPropertyRegistration unsupported[] = {
    {object, BacnetPropertyId::Reliability, readTextProperty, "unsupported"},
  };
  const BacnetServerPropertyRegistration wrongInstance[] = {
    {{static_cast<uint16_t>(BacnetObjectType::Device), kDevice.deviceInstance + 1U},
     BacnetPropertyId::Description,
     readTextProperty,
     "wrong device"},
  };
  TestTransport transport;
  BacnetServer server(transport);
  if (server.setPropertyRegistrations(unsupported, 1) ||
      !server.setPropertyRegistrations(wrongInstance, 1) || server.begin(kDevice) ||
      transport.beginCalls != 0) {
    return false;
  }
  return server.setPropertyRegistrations(nullptr, 0) && server.propertyRegistrationCount() == 0;
}

} // namespace

int main() {
  return testMinimalDeviceProfile() && testIndividualMetadataOptIns() &&
             testDeviceMetadataValidation()
           ? 0
           : 1;
}
