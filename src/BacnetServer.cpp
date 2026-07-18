// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetServer.h"

#include <cstring>

namespace {

constexpr BacnetPropertyId kDeviceProperties[] = {
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

constexpr BacnetPropertyId kAnalogValueProperties[] = {
  BacnetPropertyId::ObjectIdentifier,
  BacnetPropertyId::ObjectName,
  BacnetPropertyId::ObjectType,
  BacnetPropertyId::PresentValue,
  BacnetPropertyId::StatusFlags,
  BacnetPropertyId::EventState,
  BacnetPropertyId::OutOfService,
  BacnetPropertyId::Units,
  BacnetPropertyId::PropertyList,
};

constexpr BacnetPropertyId kAnalogInputProperties[] = {
  BacnetPropertyId::ObjectIdentifier,
  BacnetPropertyId::ObjectName,
  BacnetPropertyId::ObjectType,
  BacnetPropertyId::PresentValue,
  BacnetPropertyId::StatusFlags,
  BacnetPropertyId::EventState,
  BacnetPropertyId::OutOfService,
  BacnetPropertyId::Units,
  BacnetPropertyId::PropertyList,
};

constexpr BacnetPropertyId kBinaryInputProperties[] = {
  BacnetPropertyId::ObjectIdentifier,
  BacnetPropertyId::ObjectName,
  BacnetPropertyId::ObjectType,
  BacnetPropertyId::PresentValue,
  BacnetPropertyId::StatusFlags,
  BacnetPropertyId::EventState,
  BacnetPropertyId::OutOfService,
  BacnetPropertyId::PropertyList,
};

constexpr size_t kDevicePropertyCount = sizeof(kDeviceProperties) /
                                        sizeof(kDeviceProperties[0]);
constexpr size_t kAnalogValuePropertyCount = sizeof(kAnalogValueProperties) /
                                             sizeof(kAnalogValueProperties[0]);
constexpr size_t kAnalogInputPropertyCount = sizeof(kAnalogInputProperties) /
                                             sizeof(kAnalogInputProperties[0]);
constexpr size_t kBinaryInputPropertyCount = sizeof(kBinaryInputProperties) /
                                             sizeof(kBinaryInputProperties[0]);

bool isBaseProperty(BacnetObjectId object, BacnetPropertyId property) {
  const BacnetPropertyId* properties = nullptr;
  size_t count = 0;
  if (object.type == static_cast<uint16_t>(BacnetObjectType::AnalogValue)) {
    properties = kAnalogValueProperties;
    count = kAnalogValuePropertyCount;
  } else if (object.type == static_cast<uint16_t>(BacnetObjectType::AnalogInput)) {
    properties = kAnalogInputProperties;
    count = kAnalogInputPropertyCount;
  } else if (object.type == static_cast<uint16_t>(BacnetObjectType::BinaryInput)) {
    properties = kBinaryInputProperties;
    count = kBinaryInputPropertyCount;
  }
  for (size_t index = 0; index < count; ++index) {
    if (properties[index] == property)
      return true;
  }
  return false;
}

struct ObjectPropertyListContext {
  const BacnetServer* server = nullptr;
  BacnetObjectId object;
};

} // namespace

BacnetServer::BacnetServer(BacnetDatagramTransport& transport)
    : transport_(&transport) {}

bool BacnetServer::setTransport(BacnetDatagramTransport& transport) {
  if (running_) {
    return false;
  }
  transport_ = &transport;
  return true;
}

bool BacnetServer::begin(const BacnetServerDevice& configuredDevice,
                         uint16_t localPort) {
  if (transport_ == nullptr ||
      configuredDevice.deviceInstance > 0x003FFFFFUL ||
      !transport_->begin(localPort)) {
    return false;
  }

  device_ = configuredDevice;
  port_ = localPort;
  running_ = true;
  return true;
}

bool BacnetServer::begin(uint32_t deviceInstanceValue, uint16_t localPort) {
  BacnetServerDevice configuredDevice;
  configuredDevice.deviceInstance = deviceInstanceValue;
  return begin(configuredDevice, localPort);
}

void BacnetServer::end() {
  if (running_ && transport_ != nullptr) {
    transport_->end();
  }
  running_ = false;
}

bool BacnetServer::setAnalogValues(BacnetServerAnalogValue* analogValues,
                                   size_t count) {
  if (count == 0) {
    analogValues_ = nullptr;
    analogValueCount_ = 0;
    return true;
  }
  if (analogValues == nullptr) {
    return false;
  }

  for (size_t index = 0; index < count; ++index) {
    const BacnetServerAnalogValue& analogValue = analogValues[index];
    if (analogValue.instance > 0x003FFFFFUL || analogValue.objectName == nullptr ||
        std::strlen(analogValue.objectName) >= BacnetValue::kMaxTextLength) {
      return false;
    }
    for (size_t previous = 0; previous < index; ++previous) {
      if (analogValues[previous].instance == analogValue.instance) {
        return false;
      }
    }
  }

  analogValues_ = analogValues;
  analogValueCount_ = count;
  return true;
}

size_t BacnetServer::analogValueCount() const {
  return analogValueCount_;
}

bool BacnetServer::setAnalogInputs(BacnetServerAnalogInput* analogInputs,
                                   size_t count) {
  if (count == 0) {
    analogInputs_ = nullptr;
    analogInputCount_ = 0;
    return true;
  }
  if (analogInputs == nullptr)
    return false;
  for (size_t index = 0; index < count; ++index) {
    const BacnetServerAnalogInput& input = analogInputs[index];
    if (input.instance > 0x003FFFFFUL || input.objectName == nullptr ||
        std::strlen(input.objectName) >= BacnetValue::kMaxTextLength) {
      return false;
    }
    for (size_t previous = 0; previous < index; ++previous) {
      if (analogInputs[previous].instance == input.instance)
        return false;
    }
  }
  analogInputs_ = analogInputs;
  analogInputCount_ = count;
  return true;
}

size_t BacnetServer::analogInputCount() const {
  return analogInputCount_;
}

bool BacnetServer::setBinaryInputs(BacnetServerBinaryInput* binaryInputs,
                                   size_t count) {
  if (count == 0) {
    binaryInputs_ = nullptr;
    binaryInputCount_ = 0;
    return true;
  }
  if (binaryInputs == nullptr)
    return false;
  for (size_t index = 0; index < count; ++index) {
    const BacnetServerBinaryInput& input = binaryInputs[index];
    if (input.instance > 0x003FFFFFUL || input.objectName == nullptr ||
        std::strlen(input.objectName) >= BacnetValue::kMaxTextLength) {
      return false;
    }
    for (size_t previous = 0; previous < index; ++previous) {
      if (binaryInputs[previous].instance == input.instance)
        return false;
    }
  }
  binaryInputs_ = binaryInputs;
  binaryInputCount_ = count;
  return true;
}

size_t BacnetServer::binaryInputCount() const {
  return binaryInputCount_;
}

bool BacnetServer::setPropertyRegistrations(
  const BacnetServerPropertyRegistration* registrations,
  size_t count) {
  if (count == 0) {
    propertyRegistrations_ = nullptr;
    propertyRegistrationCount_ = 0;
    return true;
  }
  if (registrations == nullptr)
    return false;

  for (size_t index = 0; index < count; ++index) {
    const BacnetServerPropertyRegistration& registration = registrations[index];
    if (registration.provider == nullptr ||
        registration.object.instance > 0x003FFFFFUL ||
        registration.property == BacnetPropertyId::PropertyList) {
      return false;
    }
    for (size_t previous = 0; previous < index; ++previous) {
      const BacnetServerPropertyRegistration& existing = registrations[previous];
      if (existing.object.type == registration.object.type &&
          existing.object.instance == registration.object.instance &&
          existing.property == registration.property) {
        return false;
      }
    }
  }

  propertyRegistrations_ = registrations;
  propertyRegistrationCount_ = count;
  return true;
}

size_t BacnetServer::propertyRegistrationCount() const {
  return propertyRegistrationCount_;
}

bool BacnetServer::isRunning() const {
  return running_;
}

const BacnetServerDevice& BacnetServer::device() const {
  return device_;
}

uint32_t BacnetServer::deviceInstance() const {
  return device_.deviceInstance;
}

uint16_t BacnetServer::port() const {
  return port_;
}

BacnetServerPollResult BacnetServer::poll() {
  if (!running_ || transport_ == nullptr) {
    return BacnetServerPollResult::NotRunning;
  }

  uint8_t packet[kMaxDatagramSize] = {};
  BacnetIpEndpoint source;
  const size_t bytesRead = transport_->receive(packet, sizeof(packet), source);
  if (bytesRead == 0) {
    return BacnetServerPollResult::NoDatagram;
  }
  if (bytesRead > sizeof(packet)) {
    return BacnetServerPollResult::Malformed;
  }

  BacnetWhoIsRequest whoIs;
  if (BacnetProtocol::parseWhoIsRequest(packet, bytesRead, whoIs)) {
    if (!whoIs.includes(device_.deviceInstance)) {
      return BacnetServerPollResult::Ignored;
    }
    return sendIAm(source) ? BacnetServerPollResult::IAmSent
                           : BacnetServerPollResult::SendFailed;
  }

  BacnetConfirmedRequestHeader confirmedRequest;
  const auto confirmedStatus = BacnetProtocol::parseConfirmedRequestHeader(
    packet, bytesRead, confirmedRequest);
  if (confirmedStatus == BacnetConfirmedRequestParseStatus::Malformed) {
    return BacnetServerPollResult::Malformed;
  }
  if (confirmedStatus != BacnetConfirmedRequestParseStatus::Confirmed) {
    return BacnetServerPollResult::Ignored;
  }

  BacnetReadPropertyRequestHeader readProperty;
  const auto readStatus = BacnetProtocol::parseReadPropertyRequest(
    packet, bytesRead, readProperty);
  if (readStatus == BacnetReadPropertyRequestParseStatus::Malformed) {
    return BacnetServerPollResult::Malformed;
  }
  if (readStatus == BacnetReadPropertyRequestParseStatus::ReadProperty) {
    return handleReadProperty(source, readProperty);
  }

  return sendReject(source, confirmedRequest.invokeId)
           ? BacnetServerPollResult::RejectSent
           : BacnetServerPollResult::SendFailed;
}

BacnetServerPollResult BacnetServer::handleReadProperty(
  const BacnetIpEndpoint& source,
  const BacnetReadPropertyRequestHeader& request) {
  uint32_t errorClass = 0;
  uint32_t errorCode = 0;
  BacnetValue value;
  size_t responseSize = 0;
  const bool isDevice =
    request.request.object.type == static_cast<uint16_t>(BacnetObjectType::Device) &&
    request.request.object.instance == device_.deviceInstance;
  const BacnetServerAnalogValue* analogValue =
    request.request.object.type == static_cast<uint16_t>(BacnetObjectType::AnalogValue)
      ? findAnalogValue(request.request.object.instance)
      : nullptr;
  const BacnetServerAnalogInput* analogInput =
    request.request.object.type == static_cast<uint16_t>(BacnetObjectType::AnalogInput)
      ? findAnalogInput(request.request.object.instance)
      : nullptr;
  const BacnetServerBinaryInput* binaryInput =
    request.request.object.type == static_cast<uint16_t>(BacnetObjectType::BinaryInput)
      ? findBinaryInput(request.request.object.instance)
      : nullptr;

  if (!isDevice && analogValue == nullptr && analogInput == nullptr && binaryInput == nullptr) {
    errorClass = 1;
    errorCode = 31;
  } else if (isDevice) {
    if (request.request.property == BacnetPropertyId::ObjectList) {
      if (request.request.arrayIndex != kBacnetNoArrayIndex &&
          request.request.arrayIndex > analogValueCount_ + analogInputCount_ + binaryInputCount_ + 1U) {
        errorClass = 2;
        errorCode = 42;
      }
    } else if (request.request.property == BacnetPropertyId::PropertyList) {
      if (request.request.arrayIndex != kBacnetNoArrayIndex &&
          request.request.arrayIndex > kDevicePropertyCount) {
        errorClass = 2;
        errorCode = 42;
      }
    } else if (request.request.property == BacnetPropertyId::DeviceAddressBinding) {
      if (request.request.arrayIndex != kBacnetNoArrayIndex) {
        errorClass = 2;
        errorCode = 42;
      }
    } else if (request.request.arrayIndex != kBacnetNoArrayIndex) {
      errorClass = 2;
      errorCode = 42;
    } else if (!readDeviceProperty(request.request.property, value)) {
      errorClass = 2;
      errorCode = 32;
    }
  } else if (request.request.property == BacnetPropertyId::PropertyList) {
    if (request.request.arrayIndex != kBacnetNoArrayIndex &&
        request.request.arrayIndex > objectPropertyCount(request.request.object)) {
      errorClass = 2;
      errorCode = 42;
    }
  } else if (request.request.arrayIndex != kBacnetNoArrayIndex) {
    errorClass = 2;
    errorCode = 42;
  } else if ((analogValue != nullptr &&
              !readAnalogValueProperty(*analogValue, request.request.property, value)) ||
             (analogInput != nullptr &&
              !readAnalogInputProperty(*analogInput, request.request.property, value)) ||
             (binaryInput != nullptr &&
              !readBinaryInputProperty(*binaryInput, request.request.property, value))) {
    errorClass = 2;
    errorCode = 32;
  }

  uint8_t response[kMaxDatagramSize] = {};
  if (errorCode == 0) {
    if (isDevice && request.request.property == BacnetPropertyId::ObjectList) {
      responseSize = BacnetProtocol::buildReadPropertyObjectListAck(
        response,
        sizeof(response),
        request,
        analogValueCount_ + analogInputCount_ + binaryInputCount_ + 1U,
        objectListEntry,
        this);
    } else if (isDevice && request.request.property == BacnetPropertyId::PropertyList) {
      responseSize = BacnetProtocol::buildReadPropertyPropertyListAck(
        response, sizeof(response), request, kDeviceProperties, kDevicePropertyCount);
    } else if (!isDevice && request.request.property == BacnetPropertyId::PropertyList) {
      const ObjectPropertyListContext context{this, request.request.object};
      responseSize = BacnetProtocol::buildReadPropertyPropertyListAck(
        response,
        sizeof(response),
        request,
        objectPropertyCount(request.request.object),
        objectPropertyEntry,
        &context);
    } else if (isDevice && request.request.property == BacnetPropertyId::DeviceAddressBinding) {
      responseSize = BacnetProtocol::buildReadPropertyEmptyListAck(
        response, sizeof(response), request);
    } else {
      responseSize = BacnetProtocol::buildReadPropertyAck(
        response, sizeof(response), request, value);
    }
  } else {
    responseSize = BacnetProtocol::buildReadPropertyError(response, sizeof(response), request.invokeId, errorClass, errorCode);
  }
  if (responseSize == 0 || !transport_->send(source, response, responseSize)) {
    return BacnetServerPollResult::SendFailed;
  }
  return errorCode == 0 ? BacnetServerPollResult::ReadPropertyAckSent
                        : BacnetServerPollResult::ReadPropertyErrorSent;
}

bool BacnetServer::readDeviceProperty(BacnetPropertyId property,
                                      BacnetValue& value) const {
  value = BacnetValue{};
  const auto setText = [&value](const char* text) {
    if (text == nullptr) {
      return false;
    }
    const size_t length = std::strlen(text);
    if (length >= sizeof(value.text)) {
      return false;
    }
    std::memcpy(value.text, text, length + 1);
    value.textLength = length;
    value.type = BacnetValueType::CharacterString;
    return true;
  };
  switch (property) {
    case BacnetPropertyId::ObjectIdentifier:
      value.type = BacnetValueType::ObjectIdentifier;
      value.objectValue = BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::Device),
                                         device_.deviceInstance};
      return true;
    case BacnetPropertyId::ObjectName:
      return setText(device_.objectName);
    case BacnetPropertyId::VendorName:
      return setText(device_.vendorName);
    case BacnetPropertyId::ModelName:
      return setText(device_.modelName);
    case BacnetPropertyId::FirmwareRevision:
      return setText(device_.firmwareRevision);
    case BacnetPropertyId::ApplicationSoftwareVersion:
      return setText(device_.applicationSoftwareVersion != nullptr &&
                         device_.applicationSoftwareVersion[0] != '\0'
                       ? device_.applicationSoftwareVersion
                       : device_.firmwareRevision);
    case BacnetPropertyId::ObjectType:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = static_cast<uint16_t>(BacnetObjectType::Device);
      return true;
    case BacnetPropertyId::SystemStatus:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = 0;
      return true;
    case BacnetPropertyId::VendorIdentifier:
      value.type = BacnetValueType::Unsigned;
      value.unsignedValue = device_.vendorId;
      return true;
    case BacnetPropertyId::ProtocolVersion:
      value.type = BacnetValueType::Unsigned;
      value.unsignedValue = device_.protocolVersion;
      return true;
    case BacnetPropertyId::ProtocolRevision:
      value.type = BacnetValueType::Unsigned;
      value.unsignedValue = device_.protocolRevision;
      return true;
    case BacnetPropertyId::MaxApduLengthAccepted:
      value.type = BacnetValueType::Unsigned;
      value.unsignedValue = device_.maxApduLengthAccepted;
      return true;
    case BacnetPropertyId::SegmentationSupported:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = device_.segmentationSupported;
      return true;
    case BacnetPropertyId::ApduTimeout:
      value.type = BacnetValueType::Unsigned;
      value.unsignedValue = device_.apduTimeout;
      return true;
    case BacnetPropertyId::NumberOfApduRetries:
      value.type = BacnetValueType::Unsigned;
      value.unsignedValue = device_.numberOfApduRetries;
      return true;
    case BacnetPropertyId::DatabaseRevision:
      value.type = BacnetValueType::Unsigned;
      value.unsignedValue = device_.databaseRevision;
      return true;
    case BacnetPropertyId::ProtocolServicesSupported:
      value.type = BacnetValueType::BitString;
      value.bitStringValue = 1UL << 12U;
      value.bitStringBitCount = 13;
      return true;
    case BacnetPropertyId::ProtocolObjectTypesSupported:
      value.type = BacnetValueType::BitString;
      value.bitStringValue = 1UL << static_cast<uint16_t>(BacnetObjectType::Device);
      if (analogInputCount_ != 0) {
        value.bitStringValue |= 1UL << static_cast<uint16_t>(BacnetObjectType::AnalogInput);
      }
      if (analogValueCount_ != 0) {
        value.bitStringValue |= 1UL << static_cast<uint16_t>(BacnetObjectType::AnalogValue);
      }
      if (binaryInputCount_ != 0) {
        value.bitStringValue |= 1UL << static_cast<uint16_t>(BacnetObjectType::BinaryInput);
      }
      value.bitStringBitCount = 9;
      return true;
    default:
      return false;
  }
}

bool BacnetServer::readAnalogValueProperty(
  const BacnetServerAnalogValue& analogValue,
  BacnetPropertyId property,
  BacnetValue& value) const {
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::AnalogValue),
                              analogValue.instance};
  if (const BacnetServerPropertyRegistration* registration =
        findPropertyRegistration(object, property)) {
    return registration->provider(registration->context, value);
  }
  value = BacnetValue{};
  switch (property) {
    case BacnetPropertyId::ObjectIdentifier:
      value.type = BacnetValueType::ObjectIdentifier;
      value.objectValue = BacnetObjectId{
        static_cast<uint16_t>(BacnetObjectType::AnalogValue), analogValue.instance};
      return true;
    case BacnetPropertyId::ObjectName: {
      const size_t length = std::strlen(analogValue.objectName);
      std::memcpy(value.text, analogValue.objectName, length + 1U);
      value.textLength = length;
      value.type = BacnetValueType::CharacterString;
      return true;
    }
    case BacnetPropertyId::ObjectType:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = static_cast<uint16_t>(BacnetObjectType::AnalogValue);
      return true;
    case BacnetPropertyId::PresentValue:
      value.type = BacnetValueType::Real;
      value.realValue = analogValue.presentValueProvider == nullptr
                          ? analogValue.presentValue
                          : analogValue.presentValueProvider(analogValue.presentValueContext);
      return true;
    case BacnetPropertyId::StatusFlags:
      value.type = BacnetValueType::BitString;
      value.bitStringValue = analogValue.outOfService ? 1UL << 3U : 0;
      value.bitStringBitCount = 4;
      return true;
    case BacnetPropertyId::EventState:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = 0;
      return true;
    case BacnetPropertyId::OutOfService:
      value.type = BacnetValueType::Boolean;
      value.booleanValue = analogValue.outOfService;
      return true;
    case BacnetPropertyId::Units:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = analogValue.units;
      return true;
    default:
      return false;
  }
}

bool BacnetServer::readAnalogInputProperty(
  const BacnetServerAnalogInput& analogInput,
  BacnetPropertyId property,
  BacnetValue& value) const {
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::AnalogInput),
                              analogInput.instance};
  if (const BacnetServerPropertyRegistration* registration =
        findPropertyRegistration(object, property)) {
    return registration->provider(registration->context, value);
  }
  value = BacnetValue{};
  switch (property) {
    case BacnetPropertyId::ObjectIdentifier:
      value.type = BacnetValueType::ObjectIdentifier;
      value.objectValue = object;
      return true;
    case BacnetPropertyId::ObjectName: {
      const size_t length = std::strlen(analogInput.objectName);
      std::memcpy(value.text, analogInput.objectName, length + 1U);
      value.textLength = length;
      value.type = BacnetValueType::CharacterString;
      return true;
    }
    case BacnetPropertyId::ObjectType:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = static_cast<uint16_t>(BacnetObjectType::AnalogInput);
      return true;
    case BacnetPropertyId::PresentValue:
      value.type = BacnetValueType::Real;
      value.realValue = analogInput.presentValueProvider == nullptr
                          ? analogInput.presentValue
                          : analogInput.presentValueProvider(analogInput.presentValueContext);
      return true;
    case BacnetPropertyId::StatusFlags:
      value.type = BacnetValueType::BitString;
      value.bitStringValue = analogInput.outOfService ? 1UL << 3U : 0;
      value.bitStringBitCount = 4;
      return true;
    case BacnetPropertyId::EventState:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = 0;
      return true;
    case BacnetPropertyId::OutOfService:
      value.type = BacnetValueType::Boolean;
      value.booleanValue = analogInput.outOfService;
      return true;
    case BacnetPropertyId::Units:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = analogInput.units;
      return true;
    default:
      return false;
  }
}

bool BacnetServer::readBinaryInputProperty(
  const BacnetServerBinaryInput& binaryInput,
  BacnetPropertyId property,
  BacnetValue& value) const {
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::BinaryInput),
                              binaryInput.instance};
  if (const BacnetServerPropertyRegistration* registration =
        findPropertyRegistration(object, property)) {
    return registration->provider(registration->context, value);
  }
  value = BacnetValue{};
  switch (property) {
    case BacnetPropertyId::ObjectIdentifier:
      value.type = BacnetValueType::ObjectIdentifier;
      value.objectValue = object;
      return true;
    case BacnetPropertyId::ObjectName: {
      const size_t length = std::strlen(binaryInput.objectName);
      std::memcpy(value.text, binaryInput.objectName, length + 1U);
      value.textLength = length;
      value.type = BacnetValueType::CharacterString;
      return true;
    }
    case BacnetPropertyId::ObjectType:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = static_cast<uint16_t>(BacnetObjectType::BinaryInput);
      return true;
    case BacnetPropertyId::PresentValue:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = (binaryInput.presentValueProvider == nullptr
                               ? binaryInput.presentValue
                               : binaryInput.presentValueProvider(binaryInput.presentValueContext))
                              ? 1U
                              : 0U;
      return true;
    case BacnetPropertyId::StatusFlags:
      value.type = BacnetValueType::BitString;
      value.bitStringValue = binaryInput.outOfService ? 1UL << 3U : 0;
      value.bitStringBitCount = 4;
      return true;
    case BacnetPropertyId::EventState:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = 0;
      return true;
    case BacnetPropertyId::OutOfService:
      value.type = BacnetValueType::Boolean;
      value.booleanValue = binaryInput.outOfService;
      return true;
    default:
      return false;
  }
}

const BacnetServerPropertyRegistration* BacnetServer::findPropertyRegistration(
  BacnetObjectId object,
  BacnetPropertyId property) const {
  for (size_t index = 0; index < propertyRegistrationCount_; ++index) {
    const BacnetServerPropertyRegistration& registration = propertyRegistrations_[index];
    if (registration.object.type == object.type &&
        registration.object.instance == object.instance &&
        registration.property == property) {
      return &registration;
    }
  }
  return nullptr;
}

size_t BacnetServer::objectPropertyCount(BacnetObjectId object) const {
  size_t count = 0;
  if (object.type == static_cast<uint16_t>(BacnetObjectType::AnalogValue)) {
    count = kAnalogValuePropertyCount;
  } else if (object.type == static_cast<uint16_t>(BacnetObjectType::AnalogInput)) {
    count = kAnalogInputPropertyCount;
  } else if (object.type == static_cast<uint16_t>(BacnetObjectType::BinaryInput)) {
    count = kBinaryInputPropertyCount;
  }
  for (size_t index = 0; index < propertyRegistrationCount_; ++index) {
    const BacnetServerPropertyRegistration& registration = propertyRegistrations_[index];
    if (registration.object.type == object.type &&
        registration.object.instance == object.instance) {
      if (!isBaseProperty(object, registration.property))
        ++count;
    }
  }
  return count;
}

bool BacnetServer::objectPropertyAt(BacnetObjectId object,
                                    size_t index,
                                    BacnetPropertyId& property) const {
  const BacnetPropertyId* baseProperties = nullptr;
  size_t baseCount = 0;
  if (object.type == static_cast<uint16_t>(BacnetObjectType::AnalogValue)) {
    baseProperties = kAnalogValueProperties;
    baseCount = kAnalogValuePropertyCount;
  } else if (object.type == static_cast<uint16_t>(BacnetObjectType::AnalogInput)) {
    baseProperties = kAnalogInputProperties;
    baseCount = kAnalogInputPropertyCount;
  } else if (object.type == static_cast<uint16_t>(BacnetObjectType::BinaryInput)) {
    baseProperties = kBinaryInputProperties;
    baseCount = kBinaryInputPropertyCount;
  } else {
    return false;
  }
  if (index < baseCount) {
    property = baseProperties[index];
    return true;
  }
  size_t optionalIndex = index - baseCount;
  for (size_t registrationIndex = 0;
       registrationIndex < propertyRegistrationCount_;
       ++registrationIndex) {
    const BacnetServerPropertyRegistration& registration =
      propertyRegistrations_[registrationIndex];
    if (registration.object.type != object.type ||
        registration.object.instance != object.instance) {
      continue;
    }
    bool baseProperty = false;
    for (size_t baseIndex = 0; baseIndex < baseCount; ++baseIndex) {
      if (baseProperties[baseIndex] == registration.property) {
        baseProperty = true;
        break;
      }
    }
    if (baseProperty)
      continue;
    if (optionalIndex == 0) {
      property = registration.property;
      return true;
    }
    --optionalIndex;
  }
  return false;
}

bool BacnetServer::objectPropertyEntry(const void* context,
                                       size_t index,
                                       BacnetPropertyId& property) {
  const auto* listContext = static_cast<const ObjectPropertyListContext*>(context);
  return listContext != nullptr && listContext->server != nullptr &&
         listContext->server->objectPropertyAt(listContext->object, index, property);
}

const BacnetServerAnalogValue* BacnetServer::findAnalogValue(uint32_t instance) const {
  for (size_t index = 0; index < analogValueCount_; ++index) {
    if (analogValues_[index].instance == instance) {
      return &analogValues_[index];
    }
  }
  return nullptr;
}

const BacnetServerAnalogInput* BacnetServer::findAnalogInput(uint32_t instance) const {
  for (size_t index = 0; index < analogInputCount_; ++index) {
    if (analogInputs_[index].instance == instance)
      return &analogInputs_[index];
  }
  return nullptr;
}

const BacnetServerBinaryInput* BacnetServer::findBinaryInput(uint32_t instance) const {
  for (size_t index = 0; index < binaryInputCount_; ++index) {
    if (binaryInputs_[index].instance == instance)
      return &binaryInputs_[index];
  }
  return nullptr;
}

bool BacnetServer::objectListEntry(const void* context,
                                   size_t index,
                                   BacnetObjectId& object) {
  const auto* server = static_cast<const BacnetServer*>(context);
  const size_t objectCount = server == nullptr ? 0 : server->analogValueCount_ + server->analogInputCount_ + server->binaryInputCount_ + 1U;
  if (server == nullptr || index >= objectCount) {
    return false;
  }
  if (index == 0) {
    object = BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::Device),
                            server->device_.deviceInstance};
    return true;
  }
  --index;
  if (index < server->analogValueCount_) {
    object = BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::AnalogValue),
                            server->analogValues_[index].instance};
    return true;
  }
  index -= server->analogValueCount_;
  if (index < server->analogInputCount_) {
    object = BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::AnalogInput),
                            server->analogInputs_[index].instance};
    return true;
  }
  index -= server->analogInputCount_;
  object = BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::BinaryInput),
                          server->binaryInputs_[index].instance};
  return true;
}

bool BacnetServer::sendIAm(const BacnetIpEndpoint& destination) {
  uint8_t response[BacnetProtocol::kMaxIAmResponseSize] = {};
  const BacnetIAmDeviceInfo deviceInfo{
    device_.deviceInstance,
    device_.maxApduLengthAccepted,
    device_.segmentationSupported,
    device_.vendorId,
  };
  const size_t responseSize =
    BacnetProtocol::buildIAmResponse(response, sizeof(response), deviceInfo);
  return responseSize != 0 && transport_->send(destination, response, responseSize);
}

bool BacnetServer::sendReject(const BacnetIpEndpoint& destination,
                              uint8_t invokeId) {
  uint8_t response[BacnetProtocol::kRejectResponseSize] = {};
  const size_t responseSize = BacnetProtocol::buildRejectResponse(
    response, sizeof(response), invokeId, kRejectReasonUnrecognizedService);
  return responseSize != 0 && transport_->send(destination, response, responseSize);
}
