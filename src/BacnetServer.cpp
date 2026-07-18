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
  };
  const BacnetObjectId deviceObject{
    static_cast<uint16_t>(BacnetObjectType::Device), device_.deviceInstance};
  const size_t propertyCount = sizeof(kDeviceProperties) / sizeof(kDeviceProperties[0]);
  size_t responseSize = 0;
  if (request.request.object.type !=
        static_cast<uint16_t>(BacnetObjectType::Device) ||
      request.request.object.instance != device_.deviceInstance) {
    errorClass = 1;
    errorCode = 31;
  } else if (request.request.property == BacnetPropertyId::ObjectList) {
    if (request.request.arrayIndex != kBacnetNoArrayIndex &&
        request.request.arrayIndex > 1) {
      errorClass = 2;
      errorCode = 42;
    }
  } else if (request.request.property == BacnetPropertyId::PropertyList) {
    if (request.request.arrayIndex != kBacnetNoArrayIndex &&
        request.request.arrayIndex > propertyCount) {
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

  uint8_t response[kMaxDatagramSize] = {};
  if (errorCode == 0) {
    if (request.request.property == BacnetPropertyId::ObjectList) {
      responseSize = BacnetProtocol::buildReadPropertyObjectListAck(
        response, sizeof(response), request, &deviceObject, 1);
    } else if (request.request.property == BacnetPropertyId::PropertyList) {
      responseSize = BacnetProtocol::buildReadPropertyPropertyListAck(
        response, sizeof(response), request, kDeviceProperties, propertyCount);
    } else if (request.request.property == BacnetPropertyId::DeviceAddressBinding) {
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
      value.bitStringBitCount = 9;
      return true;
    default:
      return false;
  }
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
