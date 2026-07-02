// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetClient.h"

#include <cstdio>
#include <cstring>

namespace {
constexpr uint8_t kBvlcTypeBacnetIp = 0x81;
constexpr uint8_t kBvlcOriginalUnicastNpdu = 0x0A;
constexpr uint8_t kBvlcOriginalBroadcastNpdu = 0x0B;
constexpr uint8_t kNpduVersion = 0x01;
constexpr uint8_t kNpduExpectingReply = 0x04;
constexpr uint8_t kApduConfirmedRequest = 0x00;
constexpr uint8_t kApduComplexAck = 0x30;
constexpr uint8_t kApduError = 0x50;
constexpr uint8_t kApduReject = 0x60;
constexpr uint8_t kApduAbort = 0x70;
constexpr uint8_t kApduUnconfirmedRequest = 0x10;
constexpr uint8_t kServiceIAm = 0x00;
constexpr uint8_t kServiceWhoIs = 0x08;
constexpr uint8_t kServiceReadProperty = 0x0C;
constexpr uint16_t kDeviceObjectType = 8;
constexpr uint32_t kObjectInstanceMask = 0x003FFFFF;
constexpr uint8_t kApplicationTagNull = 0;
constexpr uint8_t kApplicationTagBoolean = 1;
constexpr uint8_t kApplicationTagUnsigned = 2;
constexpr uint8_t kApplicationTagSigned = 3;
constexpr uint8_t kApplicationTagReal = 4;
constexpr uint8_t kApplicationTagCharacterString = 7;
constexpr uint8_t kApplicationTagBitString = 8;
constexpr uint8_t kApplicationTagEnumerated = 9;
constexpr uint8_t kApplicationTagObjectIdentifier = 12;

bool isZeroIpAddress(const IPAddress& address) {
  return address[0] == 0 && address[1] == 0 && address[2] == 0 &&
         address[3] == 0;
}

uint16_t readUint16(const uint8_t* buffer) {
  return (static_cast<uint16_t>(buffer[0]) << 8) | buffer[1];
}

bool readApplicationValue(const uint8_t* buffer, size_t length, size_t& offset,
                          uint8_t expectedTag, uint32_t& value) {
  if (offset >= length) {
    return false;
  }

  const uint8_t tag = buffer[offset++];
  const uint8_t tagNumber = tag >> 4;
  const bool isContextTag = (tag & 0x08) != 0;
  const uint8_t valueLength = tag & 0x07;

  if (isContextTag || tagNumber != expectedTag || valueLength == 0 ||
      valueLength > 4 || valueLength == 5 || offset + valueLength > length) {
    return false;
  }

  value = 0;
  for (uint8_t i = 0; i < valueLength; ++i) {
    value = (value << 8) | buffer[offset++];
  }

  return true;
}

bool readContextValue(const uint8_t* buffer, size_t length, size_t& offset,
                      uint8_t expectedTag, uint32_t& value) {
  if (offset >= length) {
    return false;
  }

  const uint8_t tag = buffer[offset++];
  const uint8_t tagNumber = tag >> 4;
  const bool isContextTag = (tag & 0x08) != 0;
  const uint8_t valueLength = tag & 0x07;

  if (!isContextTag || tagNumber != expectedTag || valueLength == 0 ||
      valueLength > 4 || valueLength == 5 || offset + valueLength > length) {
    return false;
  }

  value = 0;
  for (uint8_t i = 0; i < valueLength; ++i) {
    value = (value << 8) | buffer[offset++];
  }

  return true;
}

bool readApplicationTagHeader(const uint8_t* buffer, size_t length,
                              size_t& offset, uint8_t expectedTag,
                              size_t& valueLength) {
  if (offset >= length) {
    return false;
  }

  const uint8_t tag = buffer[offset++];
  const uint8_t tagNumber = tag >> 4;
  const bool isContextTag = (tag & 0x08) != 0;
  uint8_t lvt = tag & 0x07;

  if (isContextTag || tagNumber != expectedTag) {
    return false;
  }

  if (lvt < 5) {
    valueLength = lvt;
  } else if (lvt == 5) {
    if (offset >= length) {
      return false;
    }
    valueLength = buffer[offset++];
  } else {
    return false;
  }

  return offset + valueLength <= length;
}

bool copyTextValue(BacnetValue& value, const char* text) {
  if (text == nullptr) {
    return false;
  }

  const size_t length = std::strlen(text);
  const size_t copyLength =
      length < (BacnetValue::kMaxTextLength - 1)
          ? length
          : (BacnetValue::kMaxTextLength - 1);

  std::memcpy(value.text, text, copyLength);
  value.text[copyLength] = '\0';
  value.textLength = copyLength;
  return true;
}

bool setTextValue(BacnetValue& value, BacnetValueType type, const char* text) {
  value.type = type;
  return copyTextValue(value, text);
}

bool parseApplicationNull(const uint8_t* buffer, size_t length, size_t& offset,
                          BacnetValue& value) {
  size_t valueLength = 0;
  if (!readApplicationTagHeader(buffer, length, offset, kApplicationTagNull,
                                valueLength) ||
      valueLength != 0) {
    return false;
  }

  return setTextValue(value, BacnetValueType::Null, "null");
}

bool parseApplicationBoolean(const uint8_t* buffer, size_t length,
                             size_t& offset, BacnetValue& value) {
  if (offset >= length) {
    return false;
  }

  const uint8_t tag = buffer[offset++];
  const uint8_t tagNumber = tag >> 4;
  const bool isContextTag = (tag & 0x08) != 0;
  if (isContextTag || tagNumber != kApplicationTagBoolean) {
    return false;
  }

  value.type = BacnetValueType::Boolean;
  value.booleanValue = (tag & 0x07) != 0;
  return copyTextValue(value, value.booleanValue ? "true" : "false");
}

bool parseApplicationUnsigned(const uint8_t* buffer, size_t length,
                              size_t& offset, uint8_t expectedTag,
                              BacnetValue& value) {
  uint32_t parsedValue = 0;
  if (!readApplicationValue(buffer, length, offset, expectedTag, parsedValue)) {
    return false;
  }

  value.type = expectedTag == kApplicationTagEnumerated
                   ? BacnetValueType::Enumerated
                   : BacnetValueType::Unsigned;
  value.unsignedValue = parsedValue;
  char text[BacnetValue::kMaxTextLength] = {};
  std::snprintf(text, sizeof(text), "%lu",
                static_cast<unsigned long>(parsedValue));
  return copyTextValue(value, text);
}

bool parseApplicationSigned(const uint8_t* buffer, size_t length,
                            size_t& offset, BacnetValue& value) {
  if (offset >= length) {
    return false;
  }

  const uint8_t tag = buffer[offset++];
  const uint8_t tagNumber = tag >> 4;
  const bool isContextTag = (tag & 0x08) != 0;
  const uint8_t valueLength = tag & 0x07;
  if (isContextTag || tagNumber != kApplicationTagSigned ||
      valueLength == 0 || valueLength > 4 || valueLength == 5 ||
      offset + valueLength > length) {
    return false;
  }

  uint32_t rawValue = 0;
  for (uint8_t i = 0; i < valueLength; ++i) {
    rawValue = (rawValue << 8) | buffer[offset++];
  }
  if ((rawValue & (1UL << ((valueLength * 8) - 1))) != 0) {
    if (valueLength < 4) {
      rawValue |= 0xFFFFFFFFUL << (valueLength * 8);
    }
  }
  const int32_t parsedValue = static_cast<int32_t>(rawValue);

  value.type = BacnetValueType::Signed;
  value.signedValue = parsedValue;
  char text[BacnetValue::kMaxTextLength] = {};
  std::snprintf(text, sizeof(text), "%ld", static_cast<long>(parsedValue));
  return copyTextValue(value, text);
}

bool parseApplicationReal(const uint8_t* buffer, size_t length, size_t& offset,
                          BacnetValue& value) {
  size_t valueLength = 0;
  if (!readApplicationTagHeader(buffer, length, offset, kApplicationTagReal,
                                valueLength) ||
      valueLength != 4 || offset + valueLength > length) {
    return false;
  }

  uint32_t raw = 0;
  for (size_t i = 0; i < valueLength; ++i) {
    raw = (raw << 8) | buffer[offset++];
  }

  float parsedValue = 0.0f;
  static_assert(sizeof(parsedValue) == sizeof(raw),
                "BACnet real parsing expects 32-bit float");
  std::memcpy(&parsedValue, &raw, sizeof(parsedValue));

  value.type = BacnetValueType::Real;
  value.realValue = parsedValue;
  char text[BacnetValue::kMaxTextLength] = {};
  std::snprintf(text, sizeof(text), "%.3f", static_cast<double>(parsedValue));
  return copyTextValue(value, text);
}

bool parseApplicationBitString(const uint8_t* buffer, size_t length,
                               size_t& offset, BacnetValue& value) {
  size_t valueLength = 0;
  if (!readApplicationTagHeader(buffer, length, offset,
                                kApplicationTagBitString, valueLength) ||
      valueLength < 2 || offset + valueLength > length) {
    return false;
  }

  const uint8_t unusedBits = buffer[offset++];
  const size_t dataLength = valueLength - 1;
  if (unusedBits > 7 || dataLength > 4) {
    return false;
  }

  const uint16_t bitCount =
      static_cast<uint16_t>((dataLength * 8) - unusedBits);
  value.type = BacnetValueType::BitString;
  value.bitStringValue = 0;
  value.bitStringBitCount =
      bitCount > UINT8_MAX ? UINT8_MAX : static_cast<uint8_t>(bitCount);

  for (uint8_t bitIndex = 0; bitIndex < value.bitStringBitCount; ++bitIndex) {
    const uint8_t sourceByte = buffer[offset + (bitIndex / 8)];
    const uint8_t sourceMask = static_cast<uint8_t>(0x80 >> (bitIndex % 8));
    if ((sourceByte & sourceMask) != 0 && bitIndex < 32) {
      value.bitStringValue |= (1UL << bitIndex);
    }
  }

  offset += dataLength;
  char text[BacnetValue::kMaxTextLength] = {};
  std::snprintf(text, sizeof(text), "bits=0x%08lx/%u",
                static_cast<unsigned long>(value.bitStringValue),
                static_cast<unsigned>(value.bitStringBitCount));
  return copyTextValue(value, text);
}

bool parseApplicationObjectIdentifier(const uint8_t* buffer, size_t length,
                                      size_t& offset, BacnetValue& value) {
  uint32_t objectIdentifier = 0;
  if (!readApplicationValue(buffer, length, offset,
                            kApplicationTagObjectIdentifier,
                            objectIdentifier)) {
    return false;
  }

  const uint16_t objectType = static_cast<uint16_t>(objectIdentifier >> 22);
  const uint32_t instance = objectIdentifier & kObjectInstanceMask;
  value.type = BacnetValueType::ObjectIdentifier;
  value.objectValue = BacnetObjectId{objectType, instance};
  char text[BacnetValue::kMaxTextLength] = {};
  std::snprintf(text, sizeof(text), "%u,%lu", objectType,
                static_cast<unsigned long>(instance));
  return copyTextValue(value, text);
}

bool appendText(char* target, size_t targetSize, size_t& used,
                const char* text) {
  if (target == nullptr || targetSize == 0 || text == nullptr) {
    return false;
  }

  const size_t textLength = std::strlen(text);
  if (used + textLength >= targetSize) {
    return false;
  }

  std::memcpy(&target[used], text, textLength);
  used += textLength;
  target[used] = '\0';
  return true;
}

bool parseApplicationObjectIdentifierList(const uint8_t* buffer, size_t length,
                                          size_t& offset, BacnetValue& value) {
  char text[BacnetValue::kMaxTextLength] = {};
  size_t used = 0;

  while (offset < length && buffer[offset] != 0x3F) {
    uint32_t objectIdentifier = 0;
    if (!readApplicationValue(buffer, length, offset,
                              kApplicationTagObjectIdentifier,
                              objectIdentifier)) {
      return false;
    }

    const uint16_t objectType = static_cast<uint16_t>(objectIdentifier >> 22);
    const uint32_t instance = objectIdentifier & kObjectInstanceMask;
    char entry[24] = {};
    std::snprintf(entry, sizeof(entry), "%s%u,%lu", used == 0 ? "" : ";",
                  objectType, static_cast<unsigned long>(instance));
    if (!appendText(text, sizeof(text), used, entry)) {
      break;
    }
  }

  value.type = BacnetValueType::ObjectIdentifierList;
  return used > 0 && copyTextValue(value, text);
}

bool parseApplicationCharacterString(const uint8_t* buffer, size_t length,
                                     size_t& offset, BacnetValue& value) {
  size_t stringLength = 0;
  if (!readApplicationTagHeader(buffer, length, offset,
                                kApplicationTagCharacterString,
                                stringLength) ||
      stringLength == 0 || offset + stringLength > length) {
    return false;
  }

  ++offset;
  --stringLength;
  const size_t copyLength =
      stringLength < (BacnetValue::kMaxTextLength - 1)
          ? stringLength
          : (BacnetValue::kMaxTextLength - 1);

  std::memcpy(value.text, &buffer[offset], copyLength);
  value.text[copyLength] = '\0';
  value.textLength = copyLength;
  value.type = BacnetValueType::CharacterString;
  offset += stringLength;
  return true;
}

bool parseUnsupportedApplicationValue(const uint8_t* buffer, size_t length,
                                      size_t& offset, BacnetValue& value) {
  if (offset >= length) {
    return false;
  }

  const uint8_t tag = buffer[offset++];
  const uint8_t tagNumber = tag >> 4;
  const bool isContextTag = (tag & 0x08) != 0;
  uint8_t lvt = tag & 0x07;
  size_t valueLength = 0;

  if (isContextTag) {
    return false;
  }
  if (lvt < 5) {
    valueLength = lvt;
  } else if (lvt == 5) {
    if (offset >= length) {
      return false;
    }
    valueLength = buffer[offset++];
  } else {
    return false;
  }
  if (offset + valueLength > length) {
    return false;
  }

  offset += valueLength;
  char text[BacnetValue::kMaxTextLength] = {};
  std::snprintf(text, sizeof(text), "unsupported application tag %u",
                tagNumber);
  return setTextValue(value, BacnetValueType::Unsupported, text);
}

bool parseReadPropertyApplicationValue(const uint8_t* buffer, size_t length,
                                       size_t& offset,
                                       BacnetPropertyId expectedProperty,
                                       BacnetValue& value,
                                       uint32_t arrayIndex =
                                           kBacnetNoArrayIndex) {
  if (offset >= length) {
    return false;
  }

  if (expectedProperty == BacnetPropertyId::ObjectList) {
    const uint8_t tagNumber = buffer[offset] >> 4;
    if (tagNumber == kApplicationTagUnsigned) {
      return parseApplicationUnsigned(buffer, length, offset,
                                      kApplicationTagUnsigned, value);
    }
    if (tagNumber == kApplicationTagObjectIdentifier) {
      if (arrayIndex != kBacnetNoArrayIndex) {
        return parseApplicationObjectIdentifier(buffer, length, offset, value);
      }
      return parseApplicationObjectIdentifierList(buffer, length, offset, value);
    }
  }

  const uint8_t tagNumber = buffer[offset] >> 4;
  if (tagNumber == kApplicationTagNull) {
    return parseApplicationNull(buffer, length, offset, value);
  }
  if (tagNumber == kApplicationTagBoolean) {
    return parseApplicationBoolean(buffer, length, offset, value);
  }
  if (tagNumber == kApplicationTagUnsigned) {
    return parseApplicationUnsigned(buffer, length, offset,
                                    kApplicationTagUnsigned, value);
  }
  if (tagNumber == kApplicationTagSigned) {
    return parseApplicationSigned(buffer, length, offset, value);
  }
  if (tagNumber == kApplicationTagReal) {
    return parseApplicationReal(buffer, length, offset, value);
  }
  if (tagNumber == kApplicationTagCharacterString) {
    return parseApplicationCharacterString(buffer, length, offset, value);
  }
  if (tagNumber == kApplicationTagBitString) {
    return parseApplicationBitString(buffer, length, offset, value);
  }
  if (tagNumber == kApplicationTagEnumerated) {
    return parseApplicationUnsigned(buffer, length, offset,
                                    kApplicationTagEnumerated, value);
  }
  if (tagNumber == kApplicationTagObjectIdentifier) {
    return parseApplicationObjectIdentifier(buffer, length, offset, value);
  }

  return parseUnsupportedApplicationValue(buffer, length, offset, value);
}

bool skipNpduAddress(const uint8_t* buffer, size_t length, size_t& offset);

bool readNpduHeader(const uint8_t* buffer, size_t length, size_t& offset) {
  if (offset >= length || buffer[offset++] != kNpduVersion || offset >= length) {
    return false;
  }

  const uint8_t npduControl = buffer[offset++];
  if ((npduControl & 0x80) != 0) {
    return false;
  }

  if ((npduControl & 0x20) != 0) {
    if (!skipNpduAddress(buffer, length, offset) || offset >= length) {
      return false;
    }
    ++offset;
  }

  if ((npduControl & 0x08) != 0 &&
      !skipNpduAddress(buffer, length, offset)) {
    return false;
  }

  return true;
}

uint32_t encodeObjectId(BacnetObjectId object) {
  return (static_cast<uint32_t>(object.type) << 22) |
         (object.instance & kObjectInstanceMask);
}

size_t writeContextUnsigned(uint8_t* buffer, size_t offset, uint8_t tagNumber,
                            uint32_t value) {
  if (value <= UINT8_MAX) {
    buffer[offset++] = static_cast<uint8_t>((tagNumber << 4) | 0x09);
    buffer[offset++] = static_cast<uint8_t>(value);
  } else if (value <= UINT16_MAX) {
    buffer[offset++] = static_cast<uint8_t>((tagNumber << 4) | 0x0A);
    buffer[offset++] = static_cast<uint8_t>(value >> 8);
    buffer[offset++] = static_cast<uint8_t>(value);
  } else {
    buffer[offset++] = static_cast<uint8_t>((tagNumber << 4) | 0x0C);
    buffer[offset++] = static_cast<uint8_t>(value >> 24);
    buffer[offset++] = static_cast<uint8_t>(value >> 16);
    buffer[offset++] = static_cast<uint8_t>(value >> 8);
    buffer[offset++] = static_cast<uint8_t>(value);
  }

  return offset;
}

size_t writeContextObjectIdentifier(uint8_t* buffer, size_t offset,
                                    uint8_t tagNumber,
                                    BacnetObjectId object) {
  const uint32_t encoded = encodeObjectId(object);
  buffer[offset++] = static_cast<uint8_t>((tagNumber << 4) | 0x0C);
  buffer[offset++] = static_cast<uint8_t>(encoded >> 24);
  buffer[offset++] = static_cast<uint8_t>(encoded >> 16);
  buffer[offset++] = static_cast<uint8_t>(encoded >> 8);
  buffer[offset++] = static_cast<uint8_t>(encoded);
  return offset;
}

bool skipNpduAddress(const uint8_t* buffer, size_t length, size_t& offset) {
  if (offset + 3 > length) {
    return false;
  }

  offset += 2;
  const uint8_t addressLength = buffer[offset++];
  if (offset + addressLength > length) {
    return false;
  }

  offset += addressLength;
  return true;
}

const char* objectTypeName(uint16_t objectType) {
  switch (objectType) {
    case static_cast<uint16_t>(BacnetObjectType::AnalogInput):
      return "analog-input";
    case static_cast<uint16_t>(BacnetObjectType::AnalogOutput):
      return "analog-output";
    case static_cast<uint16_t>(BacnetObjectType::AnalogValue):
      return "analog-value";
    case static_cast<uint16_t>(BacnetObjectType::BinaryInput):
      return "binary-input";
    case static_cast<uint16_t>(BacnetObjectType::BinaryOutput):
      return "binary-output";
    case static_cast<uint16_t>(BacnetObjectType::BinaryValue):
      return "binary-value";
    case static_cast<uint16_t>(BacnetObjectType::Device):
      return "device";
    case static_cast<uint16_t>(BacnetObjectType::MultiStateInput):
      return "multi-state-input";
    case static_cast<uint16_t>(BacnetObjectType::MultiStateOutput):
      return "multi-state-output";
    case static_cast<uint16_t>(BacnetObjectType::MultiStateValue):
      return "multi-state-value";
    default:
      return "object";
  }
}

const char* propertyName(BacnetPropertyId property) {
  switch (property) {
    case BacnetPropertyId::Description:
      return "description";
    case BacnetPropertyId::EventState:
      return "eventState";
    case BacnetPropertyId::FirmwareRevision:
      return "firmwareRevision";
    case BacnetPropertyId::ModelName:
      return "modelName";
    case BacnetPropertyId::NumberOfStates:
      return "numberOfStates";
    case BacnetPropertyId::ObjectList:
      return "objectList";
    case BacnetPropertyId::ObjectName:
      return "objectName";
    case BacnetPropertyId::ObjectType:
      return "objectType";
    case BacnetPropertyId::OutOfService:
      return "outOfService";
    case BacnetPropertyId::PresentValue:
      return "presentValue";
    case BacnetPropertyId::PriorityArray:
      return "priorityArray";
    case BacnetPropertyId::PropertyList:
      return "propertyList";
    case BacnetPropertyId::Reliability:
      return "reliability";
    case BacnetPropertyId::RelinquishDefault:
      return "relinquishDefault";
    case BacnetPropertyId::StateText:
      return "stateText";
    case BacnetPropertyId::StatusFlags:
      return "statusFlags";
    case BacnetPropertyId::VendorName:
      return "vendorName";
    default:
      return "property";
  }
}

const char* readPropertyUnexpectedStatus(const uint8_t* buffer, size_t length) {
  if (buffer == nullptr || length < 7 || buffer[0] != kBvlcTypeBacnetIp ||
      (buffer[1] != kBvlcOriginalUnicastNpdu &&
       buffer[1] != kBvlcOriginalBroadcastNpdu)) {
    return "malformed response";
  }

  size_t offset = 4;
  if (!readNpduHeader(buffer, length, offset) || offset >= length) {
    return "malformed NPDU";
  }

  const uint8_t apduType = buffer[offset] & 0xF0;
  if (apduType == kApduReject) {
    return "reject";
  }
  if (apduType == kApduAbort) {
    return "abort";
  }
  return "unexpected APDU";
}

enum class ReadPropertyResponseKind : uint8_t {
  Unrelated,
  Ack,
  Error,
  Reject,
  Abort,
};

ReadPropertyResponseKind classifyReadPropertyResponse(
    const uint8_t* buffer,
    size_t length,
    uint8_t expectedInvokeId) {
  if (buffer == nullptr || length < 8 || buffer[0] != kBvlcTypeBacnetIp ||
      (buffer[1] != kBvlcOriginalUnicastNpdu &&
       buffer[1] != kBvlcOriginalBroadcastNpdu) ||
      readUint16(&buffer[2]) != length) {
    return ReadPropertyResponseKind::Unrelated;
  }

  size_t offset = 4;
  if (!readNpduHeader(buffer, length, offset) || offset >= length) {
    return ReadPropertyResponseKind::Unrelated;
  }

  const uint8_t apduType = buffer[offset] & 0xF0;
  if (apduType == kApduComplexAck || apduType == kApduError) {
    if (offset + 3 > length || buffer[offset + 1] != expectedInvokeId ||
        buffer[offset + 2] != kServiceReadProperty) {
      return ReadPropertyResponseKind::Unrelated;
    }
    return apduType == kApduComplexAck ? ReadPropertyResponseKind::Ack
                                       : ReadPropertyResponseKind::Error;
  }

  if ((apduType == kApduReject || apduType == kApduAbort) &&
      offset + 2 <= length && buffer[offset + 1] == expectedInvokeId) {
    return apduType == kApduReject ? ReadPropertyResponseKind::Reject
                                   : ReadPropertyResponseKind::Abort;
  }

  return ReadPropertyResponseKind::Unrelated;
}
}  // namespace

bool BacnetClient::begin(uint16_t localPort) {
  localPort_ = localPort;
  running_ = udp_.begin(localPort_) == 1;
  if (running_) {
    logger_.info("BACnet/Client", "client started on UDP %u",
                 static_cast<unsigned>(localPort_));
  } else {
    logger_.error("BACnet/Client", "client start failed on UDP %u",
                  static_cast<unsigned>(localPort_));
  }
  return running_;
}

void BacnetClient::end() {
  if (running_) {
    logger_.info("BACnet/Client", "client stopped on UDP %u",
                 static_cast<unsigned>(localPort_));
  }
  udp_.stop();
  running_ = false;
}

bool BacnetClient::isRunning() const {
  return running_;
}

uint16_t BacnetClient::localPort() const {
  return localPort_;
}

BacnetLogger& BacnetClient::logger() {
  return logger_;
}

const BacnetLogger& BacnetClient::logger() const {
  return logger_;
}

bool BacnetClient::sendWhoIs(IPAddress address, uint16_t port) {
  uint8_t request[kWhoIsRequestSize] = {};
  const size_t requestSize = buildWhoIsRequest(request, sizeof(request));

  if (!running_ || requestSize == 0) {
    logger_.warn("BACnet/Discovery", "Who-Is send skipped to %s:%u",
                 address.toString().c_str(), static_cast<unsigned>(port));
    return false;
  }

  if (udp_.beginPacket(address, port) != 1) {
    logger_.warn("BACnet/Discovery", "Who-Is begin failed to %s:%u",
                 address.toString().c_str(), static_cast<unsigned>(port));
    return false;
  }

  const size_t bytesWritten = udp_.write(request, requestSize);
  const bool sent = bytesWritten == requestSize && udp_.endPacket() == 1;
  if (sent) {
    logger_.info("BACnet/Discovery", "Who-Is sent to %s:%u",
                 address.toString().c_str(), static_cast<unsigned>(port));
  } else {
    logger_.warn("BACnet/Discovery", "Who-Is send failed to %s:%u",
                 address.toString().c_str(), static_cast<unsigned>(port));
  }
  return sent;
}

bool BacnetClient::pollIAm(BacnetIAmDevice& device) {
  if (!running_) {
    return false;
  }

  const int packetSize = udp_.parsePacket();
  if (packetSize <= 0 ||
      static_cast<size_t>(packetSize) > kMaxDiscoveryPacketSize) {
    return false;
  }

  uint8_t packet[kMaxDiscoveryPacketSize] = {};
  const int bytesRead = udp_.read(packet, packetSize);
  if (bytesRead != packetSize) {
    return false;
  }

  const bool parsed =
      parseIAmResponse(packet, static_cast<size_t>(bytesRead), device);
  if (parsed) {
    device.address = udp_.remoteIP();
    logger_.info("BACnet/Discovery", "I-Am device %lu from %s vendor %u",
                 static_cast<unsigned long>(device.deviceInstance),
                 device.address.toString().c_str(),
                 static_cast<unsigned>(device.vendorId));
    if (isZeroIpAddress(device.address)) {
      logger_.warn("BACnet/Discovery",
                   "I-Am device %lu has source address 0.0.0.0",
                   static_cast<unsigned long>(device.deviceInstance));
    }
  } else {
    logger_.debug("BACnet/Discovery", "unexpected discovery packet");
  }
  return parsed;
}

bool BacnetClient::sendReadProperty(IPAddress address,
                                    const BacnetPropertyRequest& request,
                                    uint8_t invokeId, uint16_t port) {
  if (isZeroIpAddress(address)) {
    logger_.error("BACnet/ReadProperty",
                  "ReadProperty %s,%lu %s skipped invoke %u: target address "
                  "is 0.0.0.0",
                  objectTypeName(request.object.type),
                  static_cast<unsigned long>(request.object.instance),
                  propertyName(request.property),
                  static_cast<unsigned>(invokeId));
    return false;
  }

  uint8_t packet[kMaxReadPropertyRequestSize] = {};
  const size_t requestSize =
      buildReadPropertyRequest(packet, sizeof(packet), request, invokeId);

  if (!running_ || requestSize == 0) {
    logger_.warn("BACnet/ReadProperty",
                 "ReadProperty %s,%lu %s skipped invoke %u",
                 objectTypeName(request.object.type),
                 static_cast<unsigned long>(request.object.instance),
                 propertyName(request.property), static_cast<unsigned>(invokeId));
    return false;
  }

  logger_.debug("BACnet/ReadProperty",
                "ReadProperty %s,%lu %s queued invoke %u",
                objectTypeName(request.object.type),
                static_cast<unsigned long>(request.object.instance),
                propertyName(request.property), static_cast<unsigned>(invokeId));

  if (udp_.beginPacket(address, port) != 1) {
    logger_.warn("BACnet/ReadProperty",
                 "ReadProperty %s,%lu %s begin failed invoke %u",
                 objectTypeName(request.object.type),
                 static_cast<unsigned long>(request.object.instance),
                 propertyName(request.property), static_cast<unsigned>(invokeId));
    return false;
  }

  const size_t bytesWritten = udp_.write(packet, requestSize);
  const bool sent = bytesWritten == requestSize && udp_.endPacket() == 1;
  if (sent) {
    logger_.info("BACnet/ReadProperty", "ReadProperty %s,%lu %s sent invoke %u",
                 objectTypeName(request.object.type),
                 static_cast<unsigned long>(request.object.instance),
                 propertyName(request.property), static_cast<unsigned>(invokeId));
  } else {
    logger_.warn("BACnet/ReadProperty",
                 "ReadProperty %s,%lu %s send failed invoke %u",
                 objectTypeName(request.object.type),
                 static_cast<unsigned long>(request.object.instance),
                 propertyName(request.property), static_cast<unsigned>(invokeId));
  }
  return sent;
}

bool BacnetClient::sendReadProperty(IPAddress address, BacnetObjectId object,
                                    BacnetPropertyId property, uint8_t invokeId,
                                    uint16_t port, uint32_t arrayIndex) {
  return sendReadProperty(
      address, BacnetPropertyRequest{object, property, arrayIndex}, invokeId,
      port);
}

bool BacnetClient::pollReadProperty(
    BacnetValue& value, uint8_t expectedInvokeId,
    const BacnetPropertyRequest& expectedRequest) {
  return pollReadPropertyStatus(value, expectedInvokeId, expectedRequest) ==
         BacnetReadPropertyPollStatus::Ack;
}

bool BacnetClient::pollReadProperty(BacnetValue& value,
                                    uint8_t expectedInvokeId,
                                    BacnetPropertyId expectedProperty) {
  return pollReadPropertyStatus(value, expectedInvokeId, expectedProperty) ==
         BacnetReadPropertyPollStatus::Ack;
}

void BacnetClient::logReadPropertyTimeout(
    uint8_t invokeId, const BacnetPropertyRequest& request) {
  logger_.warn("BACnet/ReadProperty",
               "ReadProperty %s,%lu %s timeout invoke %u",
               objectTypeName(request.object.type),
               static_cast<unsigned long>(request.object.instance),
               propertyName(request.property), static_cast<unsigned>(invokeId));
}

BacnetReadPropertyPollStatus BacnetClient::pollReadPropertyStatus(
    BacnetValue& value, uint8_t expectedInvokeId,
    const BacnetPropertyRequest& expectedRequest) {
  return pollReadPropertyStatus(value, expectedInvokeId, expectedRequest,
                                nullptr, nullptr);
}

BacnetReadPropertyPollStatus BacnetClient::pollReadPropertyStatus(
    BacnetValue& value, uint8_t expectedInvokeId,
    const BacnetPropertyRequest& expectedRequest, uint32_t* errorClass,
    uint32_t* errorCode) {
  if (!running_) {
    return BacnetReadPropertyPollStatus::None;
  }

  const int packetSize = udp_.parsePacket();
  if (packetSize <= 0 ||
      static_cast<size_t>(packetSize) > kMaxDiscoveryPacketSize) {
    return BacnetReadPropertyPollStatus::None;
  }

  uint8_t packet[kMaxDiscoveryPacketSize] = {};
  const int bytesRead = udp_.read(packet, packetSize);
  if (bytesRead != packetSize) {
    return BacnetReadPropertyPollStatus::None;
  }

  switch (classifyReadPropertyResponse(packet, static_cast<size_t>(bytesRead),
                                       expectedInvokeId)) {
    case ReadPropertyResponseKind::Ack:
      if (parseReadPropertyAck(packet, static_cast<size_t>(bytesRead),
                               expectedInvokeId, expectedRequest, value)) {
        logger_.info("BACnet/ReadProperty",
                     "ReadProperty %s,%lu %s = %s invoke %u",
                     objectTypeName(expectedRequest.object.type),
                     static_cast<unsigned long>(expectedRequest.object.instance),
                     propertyName(expectedRequest.property), value.displayText(),
                     static_cast<unsigned>(expectedInvokeId));
        return BacnetReadPropertyPollStatus::Ack;
      }
      logger_.warn("BACnet/ReadProperty",
                   "ReadProperty %s,%lu %s decode error invoke %u",
                   objectTypeName(expectedRequest.object.type),
                   static_cast<unsigned long>(expectedRequest.object.instance),
                   propertyName(expectedRequest.property),
                   static_cast<unsigned>(expectedInvokeId));
      return BacnetReadPropertyPollStatus::DecodeError;
    case ReadPropertyResponseKind::Error:
      if (parseReadPropertyError(packet, static_cast<size_t>(bytesRead),
                                 expectedInvokeId, value, errorClass,
                                 errorCode)) {
        logger_.warn("BACnet/ReadProperty",
                     "ReadProperty %s,%lu %s error %s invoke %u",
                     objectTypeName(expectedRequest.object.type),
                     static_cast<unsigned long>(expectedRequest.object.instance),
                     propertyName(expectedRequest.property), value.displayText(),
                     static_cast<unsigned>(expectedInvokeId));
        return BacnetReadPropertyPollStatus::Error;
      }
      logger_.warn("BACnet/ReadProperty",
                   "ReadProperty %s,%lu %s malformed error invoke %u",
                   objectTypeName(expectedRequest.object.type),
                   static_cast<unsigned long>(expectedRequest.object.instance),
                   propertyName(expectedRequest.property),
                   static_cast<unsigned>(expectedInvokeId));
      return BacnetReadPropertyPollStatus::DecodeError;
    case ReadPropertyResponseKind::Reject:
      setTextValue(value, BacnetValueType::Error, "reject");
      return BacnetReadPropertyPollStatus::Reject;
    case ReadPropertyResponseKind::Abort:
      setTextValue(value, BacnetValueType::Error, "abort");
      return BacnetReadPropertyPollStatus::Abort;
    case ReadPropertyResponseKind::Unrelated:
      return BacnetReadPropertyPollStatus::None;
  }

  return BacnetReadPropertyPollStatus::None;
}

BacnetReadPropertyPollStatus BacnetClient::pollReadPropertyStatus(
    BacnetValue& value, uint8_t expectedInvokeId,
    BacnetPropertyId expectedProperty) {
  return pollReadPropertyStatus(value, expectedInvokeId, expectedProperty,
                                nullptr, nullptr);
}

BacnetReadPropertyPollStatus BacnetClient::pollReadPropertyStatus(
    BacnetValue& value, uint8_t expectedInvokeId,
    BacnetPropertyId expectedProperty, uint32_t* errorClass,
    uint32_t* errorCode) {
  if (!running_) {
    return BacnetReadPropertyPollStatus::None;
  }

  const int packetSize = udp_.parsePacket();
  if (packetSize <= 0 ||
      static_cast<size_t>(packetSize) > kMaxDiscoveryPacketSize) {
    return BacnetReadPropertyPollStatus::None;
  }

  uint8_t packet[kMaxDiscoveryPacketSize] = {};
  const int bytesRead = udp_.read(packet, packetSize);
  if (bytesRead != packetSize) {
    return BacnetReadPropertyPollStatus::None;
  }

  switch (classifyReadPropertyResponse(packet, static_cast<size_t>(bytesRead),
                                       expectedInvokeId)) {
    case ReadPropertyResponseKind::Ack:
      if (parseReadPropertyAck(packet, static_cast<size_t>(bytesRead),
                               expectedInvokeId, expectedProperty, value)) {
        logger_.info("BACnet/ReadProperty", "ReadProperty %s = %s invoke %u",
                     propertyName(expectedProperty), value.displayText(),
                     static_cast<unsigned>(expectedInvokeId));
        return BacnetReadPropertyPollStatus::Ack;
      }
      logger_.warn("BACnet/ReadProperty",
                   "ReadProperty %s decode error invoke %u",
                   propertyName(expectedProperty),
                   static_cast<unsigned>(expectedInvokeId));
      return BacnetReadPropertyPollStatus::DecodeError;
    case ReadPropertyResponseKind::Error:
      if (parseReadPropertyError(packet, static_cast<size_t>(bytesRead),
                                 expectedInvokeId, value, errorClass,
                                 errorCode)) {
        logger_.warn("BACnet/ReadProperty", "ReadProperty %s error %s invoke %u",
                     propertyName(expectedProperty), value.displayText(),
                     static_cast<unsigned>(expectedInvokeId));
        return BacnetReadPropertyPollStatus::Error;
      }
      logger_.warn("BACnet/ReadProperty",
                   "ReadProperty %s malformed error invoke %u",
                   propertyName(expectedProperty),
                   static_cast<unsigned>(expectedInvokeId));
      return BacnetReadPropertyPollStatus::DecodeError;
    case ReadPropertyResponseKind::Reject:
      setTextValue(value, BacnetValueType::Error, "reject");
      return BacnetReadPropertyPollStatus::Reject;
    case ReadPropertyResponseKind::Abort:
      setTextValue(value, BacnetValueType::Error, "abort");
      return BacnetReadPropertyPollStatus::Abort;
    case ReadPropertyResponseKind::Unrelated:
      return BacnetReadPropertyPollStatus::None;
  }

  return BacnetReadPropertyPollStatus::None;
}

size_t BacnetClient::buildWhoIsRequest(uint8_t* buffer, size_t bufferSize) {
  if (buffer == nullptr || bufferSize < kWhoIsRequestSize) {
    return 0;
  }

  buffer[0] = kBvlcTypeBacnetIp;
  buffer[1] = kBvlcOriginalBroadcastNpdu;
  buffer[2] = 0x00;
  buffer[3] = static_cast<uint8_t>(kWhoIsRequestSize);
  buffer[4] = kNpduVersion;
  buffer[5] = 0x00;
  buffer[6] = kApduUnconfirmedRequest;
  buffer[7] = kServiceWhoIs;

  return kWhoIsRequestSize;
}

bool BacnetClient::parseIAmResponse(const uint8_t* buffer, size_t length,
                                    BacnetIAmDevice& device) {
  if (buffer == nullptr || length < 14 || buffer[0] != kBvlcTypeBacnetIp ||
      (buffer[1] != kBvlcOriginalUnicastNpdu &&
       buffer[1] != kBvlcOriginalBroadcastNpdu) ||
      readUint16(&buffer[2]) != length) {
    return false;
  }

  size_t offset = 4;
  if (buffer[offset++] != kNpduVersion || offset >= length) {
    return false;
  }

  const uint8_t npduControl = buffer[offset++];
  if ((npduControl & 0x80) != 0) {
    return false;
  }

  if ((npduControl & 0x20) != 0) {
    if (!skipNpduAddress(buffer, length, offset) || offset >= length) {
      return false;
    }
    ++offset;
  }

  if ((npduControl & 0x08) != 0 &&
      !skipNpduAddress(buffer, length, offset)) {
    return false;
  }

  if (offset + 2 > length || buffer[offset++] != kApduUnconfirmedRequest ||
      buffer[offset++] != kServiceIAm) {
    return false;
  }

  uint32_t objectIdentifier = 0;
  uint32_t maxApduLengthAccepted = 0;
  uint32_t segmentationSupported = 0;
  uint32_t vendorId = 0;

  if (!readApplicationValue(buffer, length, offset, 12, objectIdentifier) ||
      (objectIdentifier >> 22) != kDeviceObjectType ||
      !readApplicationValue(buffer, length, offset, 2,
                            maxApduLengthAccepted) ||
      !readApplicationValue(buffer, length, offset, 9, segmentationSupported) ||
      segmentationSupported > UINT8_MAX ||
      !readApplicationValue(buffer, length, offset, 2, vendorId) ||
      vendorId > UINT16_MAX) {
    return false;
  }

  device.deviceInstance = objectIdentifier & kObjectInstanceMask;
  device.maxApduLengthAccepted = maxApduLengthAccepted;
  device.segmentationSupported = static_cast<uint8_t>(segmentationSupported);
  device.vendorId = static_cast<uint16_t>(vendorId);
  return true;
}

size_t BacnetClient::buildReadPropertyRequest(uint8_t* buffer,
                                              size_t bufferSize,
                                              const BacnetPropertyRequest& request,
                                              uint8_t invokeId) {
  if (buffer == nullptr || bufferSize < kMaxReadPropertyRequestSize ||
      request.object.instance > kObjectInstanceMask) {
    return 0;
  }

  size_t offset = 0;
  buffer[offset++] = kBvlcTypeBacnetIp;
  buffer[offset++] = kBvlcOriginalUnicastNpdu;
  buffer[offset++] = 0x00;
  buffer[offset++] = 0x00;
  buffer[offset++] = kNpduVersion;
  buffer[offset++] = kNpduExpectingReply;
  buffer[offset++] = kApduConfirmedRequest;
  buffer[offset++] = 0x05;
  buffer[offset++] = invokeId;
  buffer[offset++] = kServiceReadProperty;

  offset = writeContextObjectIdentifier(buffer, offset, 0, request.object);
  offset = writeContextUnsigned(buffer, offset, 1,
                                static_cast<uint32_t>(request.property));
  if (request.arrayIndex != kNoArrayIndex) {
    offset = writeContextUnsigned(buffer, offset, 2, request.arrayIndex);
  }

  buffer[2] = static_cast<uint8_t>(offset >> 8);
  buffer[3] = static_cast<uint8_t>(offset);
  return offset;
}

size_t BacnetClient::buildReadPropertyRequest(uint8_t* buffer,
                                              size_t bufferSize,
                                              BacnetObjectId object,
                                              BacnetPropertyId property,
                                              uint8_t invokeId,
                                              uint32_t arrayIndex) {
  return buildReadPropertyRequest(
      buffer, bufferSize, BacnetPropertyRequest{object, property, arrayIndex},
      invokeId);
}

bool BacnetClient::parseReadPropertyAck(const uint8_t* buffer, size_t length,
                                        uint8_t expectedInvokeId,
                                        const BacnetPropertyRequest& expectedRequest,
                                        BacnetValue& value) {
  value = BacnetValue{};
  if (buffer == nullptr || length < 17 || buffer[0] != kBvlcTypeBacnetIp ||
      buffer[1] != kBvlcOriginalUnicastNpdu || readUint16(&buffer[2]) != length) {
    return false;
  }

  size_t offset = 4;
  if (!readNpduHeader(buffer, length, offset) || offset + 3 > length ||
      (buffer[offset++] & 0xF0) != kApduComplexAck ||
      buffer[offset++] != expectedInvokeId ||
      buffer[offset++] != kServiceReadProperty) {
    return false;
  }

  uint32_t objectIdentifier = 0;
  uint32_t propertyIdentifier = 0;
  if (!readContextValue(buffer, length, offset, 0, objectIdentifier) ||
      objectIdentifier != encodeObjectId(expectedRequest.object) ||
      !readContextValue(buffer, length, offset, 1, propertyIdentifier) ||
      propertyIdentifier != static_cast<uint32_t>(expectedRequest.property)) {
    return false;
  }

  uint32_t responseArrayIndex = kBacnetNoArrayIndex;
  if (offset < length && (buffer[offset] >> 4) == 2 &&
      (buffer[offset] & 0x08) != 0) {
    if (!readContextValue(buffer, length, offset, 2, responseArrayIndex)) {
      return false;
    }
    if (expectedRequest.arrayIndex != kNoArrayIndex &&
        responseArrayIndex != expectedRequest.arrayIndex) {
      return false;
    }
  } else if (expectedRequest.arrayIndex != kNoArrayIndex) {
    return false;
  }

  if (offset >= length || buffer[offset++] != 0x3E) {
    return false;
  }

  if (!parseReadPropertyApplicationValue(buffer, length, offset,
                                         expectedRequest.property, value,
                                         responseArrayIndex)) {
    return false;
  }

  return offset < length && buffer[offset] == 0x3F;
}

bool BacnetClient::parseReadPropertyAck(const uint8_t* buffer, size_t length,
                                        uint8_t expectedInvokeId,
                                        BacnetPropertyId expectedProperty,
                                        BacnetValue& value) {
  value = BacnetValue{};
  if (buffer == nullptr || length < 17 || buffer[0] != kBvlcTypeBacnetIp ||
      buffer[1] != kBvlcOriginalUnicastNpdu || readUint16(&buffer[2]) != length) {
    return false;
  }

  size_t offset = 4;
  if (!readNpduHeader(buffer, length, offset) || offset + 3 > length ||
      (buffer[offset++] & 0xF0) != kApduComplexAck ||
      buffer[offset++] != expectedInvokeId ||
      buffer[offset++] != kServiceReadProperty) {
    return false;
  }

  uint32_t objectIdentifier = 0;
  uint32_t propertyIdentifier = 0;
  if (!readContextValue(buffer, length, offset, 0, objectIdentifier) ||
      !readContextValue(buffer, length, offset, 1, propertyIdentifier) ||
      propertyIdentifier != static_cast<uint32_t>(expectedProperty)) {
    return false;
  }

  uint32_t responseArrayIndex = kBacnetNoArrayIndex;
  if (offset < length && (buffer[offset] >> 4) == 2 &&
      (buffer[offset] & 0x08) != 0) {
    if (!readContextValue(buffer, length, offset, 2, responseArrayIndex)) {
      return false;
    }
  }

  if (offset >= length || buffer[offset++] != 0x3E) {
    return false;
  }

  if (!parseReadPropertyApplicationValue(buffer, length, offset,
                                         expectedProperty, value,
                                         responseArrayIndex)) {
    return false;
  }

  return offset < length && buffer[offset] == 0x3F;
}

bool BacnetClient::parseReadPropertyError(const uint8_t* buffer, size_t length,
                                          uint8_t expectedInvokeId,
                                          BacnetValue& value) {
  return parseReadPropertyError(buffer, length, expectedInvokeId, value,
                                nullptr, nullptr);
}

bool BacnetClient::parseReadPropertyError(const uint8_t* buffer, size_t length,
                                          uint8_t expectedInvokeId,
                                          BacnetValue& value,
                                          uint32_t* errorClass,
                                          uint32_t* errorCode) {
  value = BacnetValue{};
  if (buffer == nullptr || length < 13 || buffer[0] != kBvlcTypeBacnetIp ||
      buffer[1] != kBvlcOriginalUnicastNpdu || readUint16(&buffer[2]) != length) {
    return false;
  }

  size_t offset = 4;
  if (!readNpduHeader(buffer, length, offset) || offset + 3 > length ||
      (buffer[offset++] & 0xF0) != kApduError ||
      buffer[offset++] != expectedInvokeId ||
      buffer[offset++] != kServiceReadProperty) {
    return false;
  }

  uint32_t parsedErrorClass = 0;
  uint32_t parsedErrorCode = 0;
  char text[BacnetValue::kMaxTextLength] = {};
  if (readApplicationValue(buffer, length, offset, kApplicationTagEnumerated,
                           parsedErrorClass) &&
      readApplicationValue(buffer, length, offset, kApplicationTagEnumerated,
                           parsedErrorCode)) {
    std::snprintf(text, sizeof(text), "error class %lu code %lu",
                  static_cast<unsigned long>(parsedErrorClass),
                  static_cast<unsigned long>(parsedErrorCode));
    if (errorClass != nullptr) {
      *errorClass = parsedErrorClass;
    }
    if (errorCode != nullptr) {
      *errorCode = parsedErrorCode;
    }
  } else {
    std::snprintf(text, sizeof(text), "error");
    if (errorClass != nullptr) {
      *errorClass = 0;
    }
    if (errorCode != nullptr) {
      *errorCode = 0;
    }
  }

  return setTextValue(value, BacnetValueType::Error, text);
}
