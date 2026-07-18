// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetServer.h"

#include <cstring>

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
                         uint16_t portValue) {
  if (transport_ == nullptr ||
      configuredDevice.deviceInstance > 0x003FFFFFUL ||
      !transport_->begin(portValue)) {
    return false;
  }

  device_ = configuredDevice;
  port_ = portValue;
  running_ = true;
  return true;
}

bool BacnetServer::begin(uint32_t deviceInstanceValue, uint16_t portValue) {
  BacnetServerDevice configuredDevice;
  configuredDevice.deviceInstance = deviceInstanceValue;
  return begin(configuredDevice, portValue);
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
  static constexpr BacnetPropertyId kDeviceProperties[] = {
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
  static constexpr BacnetPropertyId kAnalogValueProperties[] = {
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
  const size_t devicePropertyCount =
    sizeof(kDeviceProperties) / sizeof(kDeviceProperties[0]);
  const size_t analogValuePropertyCount =
    sizeof(kAnalogValueProperties) / sizeof(kAnalogValueProperties[0]);
  size_t responseSize = 0;
  const bool isDevice =
    request.request.object.type == static_cast<uint16_t>(BacnetObjectType::Device) &&
    request.request.object.instance == device_.deviceInstance;
  const BacnetServerAnalogValue* analogValue =
    request.request.object.type == static_cast<uint16_t>(BacnetObjectType::AnalogValue)
      ? findAnalogValue(request.request.object.instance)
      : nullptr;

  if (!isDevice && analogValue == nullptr) {
    errorClass = 1;
    errorCode = 31;
  } else if (isDevice) {
    if (request.request.property == BacnetPropertyId::ObjectList) {
      if (request.request.arrayIndex != kBacnetNoArrayIndex &&
          request.request.arrayIndex > analogValueCount_ + 1U) {
        errorClass = 2;
        errorCode = 42;
      }
    } else if (request.request.property == BacnetPropertyId::PropertyList) {
      if (request.request.arrayIndex != kBacnetNoArrayIndex &&
          request.request.arrayIndex > devicePropertyCount) {
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
        request.request.arrayIndex > analogValuePropertyCount) {
      errorClass = 2;
      errorCode = 42;
    }
  } else if (request.request.arrayIndex != kBacnetNoArrayIndex) {
    errorClass = 2;
    errorCode = 42;
  } else if (!readAnalogValueProperty(*analogValue, request.request.property, value)) {
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
        analogValueCount_ + 1U,
        objectListEntry,
        this);
    } else if (isDevice && request.request.property == BacnetPropertyId::PropertyList) {
      responseSize = BacnetProtocol::buildReadPropertyPropertyListAck(
        response, sizeof(response), request, kDeviceProperties, devicePropertyCount);
    } else if (!isDevice && request.request.property == BacnetPropertyId::PropertyList) {
      responseSize = BacnetProtocol::buildReadPropertyPropertyListAck(
        response,
        sizeof(response),
        request,
        kAnalogValueProperties,
        analogValuePropertyCount);
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
      return setText(device_.applicationSoftwareVersion);
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
      if (analogValueCount_ != 0) {
        value.bitStringValue |= 1UL << static_cast<uint16_t>(BacnetObjectType::AnalogValue);
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
  BacnetValue& value) {
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

const BacnetServerAnalogValue* BacnetServer::findAnalogValue(uint32_t instance) const {
  for (size_t index = 0; index < analogValueCount_; ++index) {
    if (analogValues_[index].instance == instance) {
      return &analogValues_[index];
    }
  }
  return nullptr;
}

bool BacnetServer::objectListEntry(const void* context,
                                   size_t index,
                                   BacnetObjectId& object) {
  const auto* server = static_cast<const BacnetServer*>(context);
  if (server == nullptr || index > server->analogValueCount_) {
    return false;
  }
  if (index == 0) {
    object = BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::Device),
                            server->device_.deviceInstance};
    return true;
  }
  object = BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::AnalogValue),
                          server->analogValues_[index - 1U].instance};
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
