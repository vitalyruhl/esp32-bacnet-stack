// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetProtocol.h"

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
constexpr uint8_t kServiceSubscribeCov = 0x05;
constexpr uint8_t kServiceUnconfirmedCovNotification = 0x02;
constexpr uint8_t kServiceReadProperty = 0x0C;
constexpr uint8_t kServiceWriteProperty = 0x0F;
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

uint16_t readUint16(const uint8_t* buffer) {
  return (static_cast<uint16_t>(buffer[0]) << 8) | buffer[1];
}

bool readApplicationValue(const uint8_t* buffer, size_t length, size_t& offset, uint8_t expectedTag, uint32_t& value) {
  if (offset >= length) {
    return false;
  }

  const uint8_t tag = buffer[offset++];
  const uint8_t tagNumber = tag >> 4;
  const bool isContextTag = (tag & 0x08) != 0;
  const uint8_t valueLength = tag & 0x07;

  if (isContextTag || tagNumber != expectedTag || valueLength == 0 ||
      valueLength > 4 || offset + valueLength > length) {
    return false;
  }

  value = 0;
  for (uint8_t i = 0; i < valueLength; ++i) {
    value = (value << 8) | buffer[offset++];
  }

  return true;
}

bool readContextValue(const uint8_t* buffer, size_t length, size_t& offset, uint8_t expectedTag, uint32_t& value) {
  if (offset >= length) {
    return false;
  }

  const uint8_t tag = buffer[offset++];
  const uint8_t tagNumber = tag >> 4;
  const bool isContextTag = (tag & 0x08) != 0;
  const uint8_t valueLength = tag & 0x07;

  if (!isContextTag || tagNumber != expectedTag || valueLength == 0 ||
      valueLength > 4 || offset + valueLength > length) {
    return false;
  }

  value = 0;
  for (uint8_t i = 0; i < valueLength; ++i) {
    value = (value << 8) | buffer[offset++];
  }

  return true;
}

bool readApplicationTagHeader(const uint8_t* buffer, size_t length, size_t& offset, uint8_t expectedTag, size_t& valueLength) {
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

size_t writeUnsignedDecimal(char* buffer, size_t bufferSize, uint32_t value) {
  if (buffer == nullptr || bufferSize == 0) {
    return 0;
  }

  char reversed[10] = {};
  size_t digits = 0;
  do {
    reversed[digits++] = static_cast<char>('0' + (value % 10));
    value /= 10;
  } while (value != 0 && digits < sizeof(reversed));

  if (digits + 1 > bufferSize) {
    return 0;
  }

  for (size_t i = 0; i < digits; ++i) {
    buffer[i] = reversed[digits - 1 - i];
  }
  buffer[digits] = '\0';
  return digits;
}

size_t writeSignedDecimal(char* buffer, size_t bufferSize, int32_t value) {
  if (buffer == nullptr || bufferSize == 0) {
    return 0;
  }

  size_t used = 0;
  uint32_t magnitude = 0;
  if (value < 0) {
    if (bufferSize < 2) {
      return 0;
    }
    buffer[used++] = '-';
    magnitude = static_cast<uint32_t>(-(static_cast<int64_t>(value)));
  } else {
    magnitude = static_cast<uint32_t>(value);
  }

  const size_t written =
    writeUnsignedDecimal(&buffer[used], bufferSize - used, magnitude);
  return written == 0 ? 0 : (used + written);
}

bool writeFixed3Decimal(char* buffer, size_t bufferSize, float value) {
  if (buffer == nullptr || bufferSize == 0) {
    return false;
  }

  size_t used = 0;
  if (value < 0.0f) {
    if (bufferSize < 2) {
      return false;
    }
    buffer[used++] = '-';
    value = -value;
  }

  const uint32_t scaled = static_cast<uint32_t>((value * 1000.0f) + 0.5f);
  const uint32_t integerPart = scaled / 1000U;
  const uint32_t fractionalPart = scaled % 1000U;

  const size_t integerChars =
    writeUnsignedDecimal(&buffer[used], bufferSize - used, integerPart);
  if (integerChars == 0) {
    return false;
  }
  used += integerChars;

  if (used + 4 >= bufferSize) {
    return false;
  }
  buffer[used++] = '.';
  buffer[used++] = static_cast<char>('0' + ((fractionalPart / 100U) % 10U));
  buffer[used++] = static_cast<char>('0' + ((fractionalPart / 10U) % 10U));
  buffer[used++] = static_cast<char>('0' + (fractionalPart % 10U));
  buffer[used] = '\0';
  return true;
}

bool writeHex32(char* buffer, size_t bufferSize, uint32_t value) {
  if (buffer == nullptr || bufferSize < 9) {
    return false;
  }

  static const char kHex[] = "0123456789abcdef";
  for (int i = 0; i < 8; ++i) {
    const uint8_t shift = static_cast<uint8_t>((7 - i) * 4);
    buffer[i] = kHex[(value >> shift) & 0x0FU];
  }
  buffer[8] = '\0';
  return true;
}

bool parseApplicationNull(const uint8_t* buffer, size_t length, size_t& offset, BacnetValue& value) {
  size_t valueLength = 0;
  if (!readApplicationTagHeader(buffer, length, offset, kApplicationTagNull, valueLength) ||
      valueLength != 0) {
    return false;
  }

  return setTextValue(value, BacnetValueType::Null, "null");
}

bool parseApplicationBoolean(const uint8_t* buffer, size_t length, size_t& offset, BacnetValue& value) {
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

bool parseApplicationUnsigned(const uint8_t* buffer, size_t length, size_t& offset, uint8_t expectedTag, BacnetValue& value) {
  uint32_t parsedValue = 0;
  if (!readApplicationValue(buffer, length, offset, expectedTag, parsedValue)) {
    return false;
  }

  value.type = expectedTag == kApplicationTagEnumerated
                 ? BacnetValueType::Enumerated
                 : BacnetValueType::Unsigned;
  value.unsignedValue = parsedValue;
  char text[16] = {};
  if (writeUnsignedDecimal(text, sizeof(text), parsedValue) == 0) {
    return false;
  }
  return copyTextValue(value, text);
}

bool parseApplicationSigned(const uint8_t* buffer, size_t length, size_t& offset, BacnetValue& value) {
  if (offset >= length) {
    return false;
  }

  const uint8_t tag = buffer[offset++];
  const uint8_t tagNumber = tag >> 4;
  const bool isContextTag = (tag & 0x08) != 0;
  const uint8_t valueLength = tag & 0x07;
  if (isContextTag || tagNumber != kApplicationTagSigned ||
      valueLength == 0 || valueLength > 4 ||
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
  char text[16] = {};
  if (writeSignedDecimal(text, sizeof(text), parsedValue) == 0) {
    return false;
  }
  return copyTextValue(value, text);
}

bool parseApplicationReal(const uint8_t* buffer, size_t length, size_t& offset, BacnetValue& value) {
  size_t valueLength = 0;
  if (!readApplicationTagHeader(buffer, length, offset, kApplicationTagReal, valueLength) ||
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
  char text[24] = {};
  if (!writeFixed3Decimal(text, sizeof(text), parsedValue)) {
    return false;
  }
  return copyTextValue(value, text);
}

bool parseApplicationBitString(const uint8_t* buffer, size_t length, size_t& offset, BacnetValue& value) {
  size_t valueLength = 0;
  if (!readApplicationTagHeader(buffer, length, offset, kApplicationTagBitString, valueLength) ||
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
  char hex[9] = {};
  if (!writeHex32(hex, sizeof(hex), value.bitStringValue)) {
    return false;
  }
  char bits[8] = {};
  if (writeUnsignedDecimal(bits, sizeof(bits), value.bitStringBitCount) == 0) {
    return false;
  }
  char text[32] = "bits=0x";
  size_t used = std::strlen(text);
  const size_t hexLen = std::strlen(hex);
  const size_t bitsLen = std::strlen(bits);
  if (used + hexLen + 1 + bitsLen + 1 > sizeof(text)) {
    return false;
  }
  std::memcpy(&text[used], hex, hexLen);
  used += hexLen;
  text[used++] = '/';
  std::memcpy(&text[used], bits, bitsLen);
  used += bitsLen;
  text[used] = '\0';
  return copyTextValue(value, text);
}

bool parseApplicationObjectIdentifier(const uint8_t* buffer, size_t length, size_t& offset, BacnetValue& value) {
  uint32_t objectIdentifier = 0;
  if (!readApplicationValue(buffer, length, offset, kApplicationTagObjectIdentifier, objectIdentifier)) {
    return false;
  }

  const uint16_t objectType = static_cast<uint16_t>(objectIdentifier >> 22);
  const uint32_t instance = objectIdentifier & kObjectInstanceMask;
  value.type = BacnetValueType::ObjectIdentifier;
  value.objectValue = BacnetObjectId{objectType, instance};
  char text[24] = {};
  const size_t typeChars =
    writeUnsignedDecimal(text, sizeof(text), static_cast<uint32_t>(objectType));
  if (typeChars == 0 || typeChars + 2 > sizeof(text)) {
    return false;
  }
  text[typeChars] = ',';
  if (writeUnsignedDecimal(&text[typeChars + 1], sizeof(text) - typeChars - 1, instance) == 0) {
    return false;
  }
  return copyTextValue(value, text);
}

bool appendText(char* target, size_t targetSize, size_t& used, const char* text) {
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

bool appendUnsignedDecimal(char* target, size_t targetSize, size_t& used, uint32_t value) {
  char digits[16] = {};
  const size_t digitCount = writeUnsignedDecimal(digits, sizeof(digits), value);
  return digitCount != 0 && appendText(target, targetSize, used, digits);
}

bool parseApplicationObjectIdentifierList(const uint8_t* buffer, size_t length, size_t& offset, BacnetValue& value) {
  char* text = value.text;
  text[0] = '\0';
  size_t used = 0;

  while (offset < length && buffer[offset] != 0x3F) {
    uint32_t objectIdentifier = 0;
    if (!readApplicationValue(buffer, length, offset, kApplicationTagObjectIdentifier, objectIdentifier)) {
      return false;
    }

    const uint16_t objectType = static_cast<uint16_t>(objectIdentifier >> 22);
    const uint32_t instance = objectIdentifier & kObjectInstanceMask;
    if (used > 0 && !appendText(text, BacnetValue::kMaxTextLength, used, ";")) {
      break;
    }
    if (!appendUnsignedDecimal(text, BacnetValue::kMaxTextLength, used, static_cast<uint32_t>(objectType)) ||
        !appendText(text, BacnetValue::kMaxTextLength, used, ",") ||
        !appendUnsignedDecimal(text, BacnetValue::kMaxTextLength, used, instance)) {
      break;
    }
  }

  value.type = BacnetValueType::ObjectIdentifierList;
  value.textLength = used;
  return used > 0;
}

bool parseApplicationCharacterString(const uint8_t* buffer, size_t length, size_t& offset, BacnetValue& value) {
  size_t stringLength = 0;
  if (!readApplicationTagHeader(buffer, length, offset, kApplicationTagCharacterString, stringLength) ||
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

bool parseUnsupportedApplicationValue(const uint8_t* buffer, size_t length, size_t& offset, BacnetValue& value) {
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
  char text[48] = "unsupported application tag ";
  size_t used = std::strlen(text);
  if (!appendUnsignedDecimal(text, sizeof(text), used, tagNumber)) {
    return false;
  }
  return setTextValue(value, BacnetValueType::Unsupported, text);
}

bool parseReadPropertyApplicationValue(const uint8_t* buffer, size_t length, size_t& offset, BacnetPropertyId expectedProperty, BacnetValue& value, uint32_t arrayIndex = kBacnetNoArrayIndex) {
  if (offset >= length) {
    return false;
  }

  if (expectedProperty == BacnetPropertyId::ObjectList) {
    const uint8_t tagNumber = buffer[offset] >> 4;
    if (tagNumber == kApplicationTagUnsigned) {
      return parseApplicationUnsigned(buffer, length, offset, kApplicationTagUnsigned, value);
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
    return parseApplicationUnsigned(buffer, length, offset, kApplicationTagUnsigned, value);
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
    return parseApplicationUnsigned(buffer, length, offset, kApplicationTagEnumerated, value);
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

size_t writeContextUnsigned(uint8_t* buffer, size_t offset, uint8_t tagNumber, uint32_t value) {
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

size_t contextUnsignedSize(uint32_t value) {
  if (value <= UINT8_MAX)
    return 2;
  if (value <= UINT16_MAX)
    return 3;
  return 5;
}

size_t writeContextBoolean(uint8_t* buffer,
                           size_t offset,
                           uint8_t tagNumber,
                           bool value) {
  buffer[offset++] = static_cast<uint8_t>((tagNumber << 4) | 0x09);
  buffer[offset++] = value ? 1 : 0;
  return offset;
}

size_t writeContextObjectIdentifier(uint8_t* buffer, size_t offset, uint8_t tagNumber, BacnetObjectId object) {
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
    case BacnetPropertyId::CovIncrement:
      return "covIncrement";
    case BacnetPropertyId::Description:
      return "description";
    case BacnetPropertyId::EventState:
      return "eventState";
    case BacnetPropertyId::FirmwareRevision:
      return "firmwareRevision";
    case BacnetPropertyId::MaxPresentValue:
      return "maxPresentValue";
    case BacnetPropertyId::MinPresentValue:
      return "minPresentValue";
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
    case BacnetPropertyId::Resolution:
      return "resolution";
    case BacnetPropertyId::StateText:
      return "stateText";
    case BacnetPropertyId::StatusFlags:
      return "statusFlags";
    case BacnetPropertyId::Units:
      return "units";
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

} // namespace

BacnetReadPropertyResponseKind BacnetProtocol::classifyReadPropertyResponse(
  const uint8_t* buffer,
  size_t length,
  uint8_t expectedInvokeId) {
  if (buffer == nullptr || length < 8 || buffer[0] != kBvlcTypeBacnetIp ||
      (buffer[1] != kBvlcOriginalUnicastNpdu &&
       buffer[1] != kBvlcOriginalBroadcastNpdu) ||
      readUint16(&buffer[2]) != length) {
    return BacnetReadPropertyResponseKind::Unrelated;
  }

  size_t offset = 4;
  if (!readNpduHeader(buffer, length, offset) || offset >= length) {
    return BacnetReadPropertyResponseKind::Unrelated;
  }

  const uint8_t apduType = buffer[offset] & 0xF0;
  if (apduType == kApduComplexAck || apduType == kApduError) {
    if (offset + 3 > length || buffer[offset + 1] != expectedInvokeId ||
        buffer[offset + 2] != kServiceReadProperty) {
      return BacnetReadPropertyResponseKind::Unrelated;
    }
    return apduType == kApduComplexAck ? BacnetReadPropertyResponseKind::Ack
                                       : BacnetReadPropertyResponseKind::Error;
  }

  if ((apduType == kApduReject || apduType == kApduAbort) &&
      offset + 2 <= length && buffer[offset + 1] == expectedInvokeId) {
    return apduType == kApduReject ? BacnetReadPropertyResponseKind::Reject
                                   : BacnetReadPropertyResponseKind::Abort;
  }

  return BacnetReadPropertyResponseKind::Unrelated;
}

BacnetWritePropertyResponseKind BacnetProtocol::classifyWritePropertyResponse(
  const uint8_t* buffer, size_t length, uint8_t expectedInvokeId) {
  if (buffer == nullptr || length < 8 || buffer[0] != kBvlcTypeBacnetIp ||
      (buffer[1] != kBvlcOriginalUnicastNpdu &&
       buffer[1] != kBvlcOriginalBroadcastNpdu) ||
      readUint16(&buffer[2]) != length) {
    return BacnetWritePropertyResponseKind::Unrelated;
  }
  size_t offset = 4;
  if (!readNpduHeader(buffer, length, offset) || offset >= length) {
    return BacnetWritePropertyResponseKind::Unrelated;
  }
  const uint8_t apduType = buffer[offset] & 0xF0;
  if (apduType == 0x20 || apduType == kApduError) {
    if (offset + 3 > length || buffer[offset + 1] != expectedInvokeId ||
        buffer[offset + 2] != kServiceWriteProperty) {
      return BacnetWritePropertyResponseKind::Unrelated;
    }
    return apduType == 0x20 ? BacnetWritePropertyResponseKind::Ack
                            : BacnetWritePropertyResponseKind::Error;
  }
  if ((apduType == kApduReject || apduType == kApduAbort) &&
      offset + 2 <= length && buffer[offset + 1] == expectedInvokeId) {
    return apduType == kApduReject ? BacnetWritePropertyResponseKind::Reject
                                   : BacnetWritePropertyResponseKind::Abort;
  }
  return BacnetWritePropertyResponseKind::Unrelated;
}

size_t BacnetProtocol::buildWhoIsRequest(uint8_t* buffer, size_t bufferSize) {
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

bool BacnetProtocol::parseWhoIsRequest(const uint8_t* buffer,
                                       size_t length,
                                       BacnetWhoIsRequest& request) {
  request = BacnetWhoIsRequest{};
  if (buffer == nullptr || length < kWhoIsRequestSize ||
      buffer[0] != kBvlcTypeBacnetIp ||
      (buffer[1] != kBvlcOriginalUnicastNpdu &&
       buffer[1] != kBvlcOriginalBroadcastNpdu) ||
      readUint16(&buffer[2]) != length) {
    return false;
  }

  size_t offset = 4;
  if (!readNpduHeader(buffer, length, offset) || offset + 2 > length ||
      buffer[offset++] != kApduUnconfirmedRequest ||
      buffer[offset++] != kServiceWhoIs) {
    return false;
  }

  if (offset == length) {
    return true;
  }

  uint32_t lowDeviceInstance = 0;
  uint32_t highDeviceInstance = 0;
  if (!readContextValue(buffer, length, offset, 0, lowDeviceInstance) ||
      !readContextValue(buffer, length, offset, 1, highDeviceInstance) ||
      offset != length || lowDeviceInstance > highDeviceInstance ||
      highDeviceInstance > kObjectInstanceMask) {
    return false;
  }

  request.hasDeviceInstanceRange = true;
  request.lowDeviceInstance = lowDeviceInstance;
  request.highDeviceInstance = highDeviceInstance;
  return true;
}

BacnetConfirmedRequestParseStatus
BacnetProtocol::parseConfirmedRequestHeader(
  const uint8_t* buffer,
  size_t length,
  BacnetConfirmedRequestHeader& header) {
  header = BacnetConfirmedRequestHeader{};
  if (buffer == nullptr || length < 4 || buffer[0] != kBvlcTypeBacnetIp ||
      buffer[1] != kBvlcOriginalUnicastNpdu ||
      readUint16(&buffer[2]) != length) {
    return BacnetConfirmedRequestParseStatus::Malformed;
  }

  size_t offset = 4;
  if (!readNpduHeader(buffer, length, offset) || offset >= length) {
    return BacnetConfirmedRequestParseStatus::Malformed;
  }

  if ((buffer[offset] & 0xF0) != kApduConfirmedRequest) {
    return BacnetConfirmedRequestParseStatus::Unrelated;
  }

  if (offset + 4 > length) {
    return BacnetConfirmedRequestParseStatus::Malformed;
  }

  header.invokeId = buffer[offset + 2];
  header.serviceChoice = buffer[offset + 3];
  return BacnetConfirmedRequestParseStatus::Confirmed;
}

size_t BacnetProtocol::buildIAmResponse(uint8_t* buffer,
                                        size_t bufferSize,
                                        const BacnetIAmDeviceInfo& device) {
  const auto applicationUnsignedSize = [](uint32_t value) -> size_t {
    return value <= UINT8_MAX    ? 2
           : value <= UINT16_MAX ? 3
           : value <= 0xFFFFFFU  ? 4
                                 : 5;
  };
  const size_t required = 13 +
                          applicationUnsignedSize(device.maxApduLengthAccepted) +
                          applicationUnsignedSize(device.segmentationSupported) +
                          applicationUnsignedSize(device.vendorId);
  if (buffer == nullptr || bufferSize < required ||
      device.deviceInstance > kObjectInstanceMask) {
    return 0;
  }

  const auto writeApplicationUnsigned = [&](uint8_t tag,
                                            uint32_t value,
                                            size_t& offset) {
    const uint8_t valueLength = value <= UINT8_MAX    ? 1
                                : value <= UINT16_MAX ? 2
                                : value <= 0xFFFFFFU  ? 3
                                                      : 4;
    buffer[offset++] = static_cast<uint8_t>((tag << 4) | valueLength);
    for (uint8_t index = 0; index < valueLength; ++index) {
      buffer[offset++] = static_cast<uint8_t>(
        value >> ((valueLength - index - 1U) * 8U));
    }
  };

  size_t offset = 0;
  buffer[offset++] = kBvlcTypeBacnetIp;
  buffer[offset++] = kBvlcOriginalUnicastNpdu;
  buffer[offset++] = 0;
  buffer[offset++] = 0;
  buffer[offset++] = kNpduVersion;
  buffer[offset++] = 0;
  buffer[offset++] = kApduUnconfirmedRequest;
  buffer[offset++] = kServiceIAm;
  buffer[offset++] = 0xC4;
  const uint32_t objectIdentifier =
    (static_cast<uint32_t>(kDeviceObjectType) << 22) |
    device.deviceInstance;
  buffer[offset++] = static_cast<uint8_t>(objectIdentifier >> 24);
  buffer[offset++] = static_cast<uint8_t>(objectIdentifier >> 16);
  buffer[offset++] = static_cast<uint8_t>(objectIdentifier >> 8);
  buffer[offset++] = static_cast<uint8_t>(objectIdentifier);
  writeApplicationUnsigned(kApplicationTagUnsigned,
                           device.maxApduLengthAccepted,
                           offset);
  writeApplicationUnsigned(kApplicationTagEnumerated,
                           device.segmentationSupported,
                           offset);
  writeApplicationUnsigned(kApplicationTagUnsigned, device.vendorId, offset);
  buffer[2] = static_cast<uint8_t>(offset >> 8);
  buffer[3] = static_cast<uint8_t>(offset);
  return offset;
}

size_t BacnetProtocol::buildRejectResponse(uint8_t* buffer,
                                           size_t bufferSize,
                                           uint8_t invokeId,
                                           uint8_t reason) {
  if (buffer == nullptr || bufferSize < kRejectResponseSize) {
    return 0;
  }

  buffer[0] = kBvlcTypeBacnetIp;
  buffer[1] = kBvlcOriginalUnicastNpdu;
  buffer[2] = 0;
  buffer[3] = static_cast<uint8_t>(kRejectResponseSize);
  buffer[4] = kNpduVersion;
  buffer[5] = 0;
  buffer[6] = kApduReject;
  buffer[7] = invokeId;
  buffer[8] = reason;
  return kRejectResponseSize;
}

size_t BacnetProtocol::encodeApplicationValue(uint8_t* buffer, size_t bufferSize, const BacnetValue& value) {
  if (buffer == nullptr || bufferSize == 0) {
    return 0;
  }

  const auto writeUnsigned = [&](uint8_t tag, uint32_t raw) -> size_t {
    const uint8_t length = raw <= 0xFFU ? 1 : raw <= 0xFFFFU   ? 2
                                            : raw <= 0xFFFFFFU ? 3
                                                               : 4;
    if (bufferSize < static_cast<size_t>(length + 1)) {
      return 0;
    }
    buffer[0] = static_cast<uint8_t>((tag << 4) | length);
    for (uint8_t index = 0; index < length; ++index) {
      buffer[1 + index] = static_cast<uint8_t>(raw >> ((length - index - 1) * 8));
    }
    return length + 1;
  };

  const auto writeSigned = [&](int32_t raw) -> size_t {
    uint8_t length = 4;
    if (raw >= -128 && raw <= 127) {
      length = 1;
    } else if (raw >= -32768 && raw <= 32767) {
      length = 2;
    } else if (raw >= -8388608 && raw <= 8388607) {
      length = 3;
    }
    if (bufferSize < static_cast<size_t>(length + 1)) {
      return 0;
    }
    buffer[0] = static_cast<uint8_t>((kApplicationTagSigned << 4) | length);
    const uint32_t bits = static_cast<uint32_t>(raw);
    for (uint8_t index = 0; index < length; ++index) {
      buffer[1 + index] = static_cast<uint8_t>(bits >> ((length - index - 1) * 8));
    }
    return length + 1;
  };

  switch (value.type) {
    case BacnetValueType::Null:
      buffer[0] = 0x00;
      return 1;
    case BacnetValueType::Boolean:
      buffer[0] = static_cast<uint8_t>(0x10 | (value.booleanValue ? 1 : 0));
      return 1;
    case BacnetValueType::Unsigned:
      return writeUnsigned(kApplicationTagUnsigned, value.unsignedValue);
    case BacnetValueType::Signed:
      return writeSigned(value.signedValue);
    case BacnetValueType::Enumerated:
      return writeUnsigned(kApplicationTagEnumerated, value.unsignedValue);
    case BacnetValueType::Real: {
      if (bufferSize < 5) {
        return 0;
      }
      uint32_t raw = 0;
      std::memcpy(&raw, &value.realValue, sizeof(raw));
      buffer[0] = 0x44;
      buffer[1] = static_cast<uint8_t>(raw >> 24);
      buffer[2] = static_cast<uint8_t>(raw >> 16);
      buffer[3] = static_cast<uint8_t>(raw >> 8);
      buffer[4] = static_cast<uint8_t>(raw);
      return 5;
    }
    case BacnetValueType::CharacterString: {
      if (value.textLength >= BacnetValue::kMaxTextLength ||
          value.textLength > 253) {
        return 0;
      }
      const size_t payloadLength = value.textLength + 1;
      const size_t headerLength = payloadLength <= 4 ? 1 : 2;
      if (bufferSize < headerLength + payloadLength) {
        return 0;
      }
      buffer[0] = static_cast<uint8_t>((kApplicationTagCharacterString << 4) |
                                       (payloadLength <= 4 ? payloadLength : 5));
      size_t offset = 1;
      if (payloadLength > 4) {
        buffer[offset++] = static_cast<uint8_t>(payloadLength);
      }
      buffer[offset++] = 0; // ANSI X3.4 / UTF-8 compatible character set.
      std::memcpy(buffer + offset, value.text, value.textLength);
      return offset + value.textLength;
    }
    case BacnetValueType::ObjectIdentifier: {
      if (value.objectValue.type > 1023 ||
          value.objectValue.instance > kObjectInstanceMask || bufferSize < 5) {
        return 0;
      }
      const uint32_t object = encodeObjectId(value.objectValue);
      buffer[0] = 0xC4;
      buffer[1] = static_cast<uint8_t>(object >> 24);
      buffer[2] = static_cast<uint8_t>(object >> 16);
      buffer[3] = static_cast<uint8_t>(object >> 8);
      buffer[4] = static_cast<uint8_t>(object);
      return 5;
    }
    default:
      return 0;
  }
}

size_t BacnetProtocol::buildSubscribeCovRequest(uint8_t* buffer,
                                                size_t bufferSize,
                                                uint32_t processId,
                                                BacnetObjectId object,
                                                uint32_t lifetimeSeconds,
                                                bool issueConfirmedNotifications) {
  const size_t required = 10 + contextUnsignedSize(processId) + 5 + 2 +
                          contextUnsignedSize(lifetimeSeconds);
  if (buffer == nullptr || bufferSize < required ||
      object.instance > kObjectInstanceMask) {
    return 0;
  }
  size_t offset = 0;
  buffer[offset++] = kBvlcTypeBacnetIp;
  buffer[offset++] = kBvlcOriginalUnicastNpdu;
  buffer[offset++] = 0;
  buffer[offset++] = 0;
  buffer[offset++] = kNpduVersion;
  buffer[offset++] = kNpduExpectingReply;
  buffer[offset++] = kApduConfirmedRequest;
  buffer[offset++] = 0x05;
  buffer[offset++] = 0;
  buffer[offset++] = kServiceSubscribeCov;
  offset = writeContextUnsigned(buffer, offset, 0, processId);
  offset = writeContextObjectIdentifier(buffer, offset, 1, object);
  offset = writeContextBoolean(buffer, offset, 2, issueConfirmedNotifications);
  offset = writeContextUnsigned(buffer, offset, 3, lifetimeSeconds);
  buffer[2] = static_cast<uint8_t>(offset >> 8);
  buffer[3] = static_cast<uint8_t>(offset);
  return offset;
}

BacnetSubscribeCovResponseKind BacnetProtocol::classifySubscribeCovResponse(
  const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, uint8_t* rejectReason) {
  if (rejectReason != nullptr)
    *rejectReason = 0xFF;
  if (buffer == nullptr || length < 8 || buffer[0] != kBvlcTypeBacnetIp ||
      buffer[1] != kBvlcOriginalUnicastNpdu || readUint16(&buffer[2]) != length)
    return BacnetSubscribeCovResponseKind::None;
  size_t offset = 4;
  if (!readNpduHeader(buffer, length, offset) || offset + 2 > length || buffer[offset + 1] != expectedInvokeId)
    return BacnetSubscribeCovResponseKind::None;
  const uint8_t apduType = buffer[offset] & 0xF0;
  if (apduType == 0x20 && offset + 3 <= length && buffer[offset + 2] == kServiceSubscribeCov)
    return BacnetSubscribeCovResponseKind::Ack;
  if (apduType == kApduError && offset + 3 <= length && buffer[offset + 2] == kServiceSubscribeCov)
    return BacnetSubscribeCovResponseKind::Error;
  if (apduType == kApduReject) {
    if (rejectReason != nullptr && offset + 3 <= length)
      *rejectReason = buffer[offset + 2];
    return BacnetSubscribeCovResponseKind::Reject;
  }
  if (apduType == kApduAbort)
    return BacnetSubscribeCovResponseKind::Abort;
  return BacnetSubscribeCovResponseKind::None;
}

const char* BacnetProtocol::rejectReasonText(uint8_t rejectReason) {
  switch (rejectReason) {
    case 0:
      return "other";
    case 1:
      return "buffer-overflow";
    case 2:
      return "inconsistent-parameters";
    case 3:
      return "invalid-parameter-data-type";
    case 4:
      return "invalid-tag";
    case 5:
      return "missing-required-parameter";
    case 6:
      return "parameter-out-of-range";
    case 7:
      return "too-many-arguments";
    case 8:
      return "undefined-enumeration";
    case 9:
      return "unrecognized-service";
    default:
      return "unknown";
  }
}

bool BacnetProtocol::parseCovNotification(const uint8_t* buffer,
                                          size_t length,
                                          BacnetCovNotification& notification) {
  notification = BacnetCovNotification{};
  if (buffer == nullptr || length < 20 || buffer[0] != kBvlcTypeBacnetIp ||
      (buffer[1] != kBvlcOriginalUnicastNpdu && buffer[1] != kBvlcOriginalBroadcastNpdu) ||
      readUint16(&buffer[2]) != length) {
    return false;
  }
  size_t offset = 4;
  if (!readNpduHeader(buffer, length, offset) || offset + 2 > length ||
      buffer[offset++] != kApduUnconfirmedRequest ||
      buffer[offset++] != kServiceUnconfirmedCovNotification) {
    return false;
  }
  uint32_t processId = 0;
  uint32_t initiatingDevice = 0;
  uint32_t monitoredObject = 0;
  uint32_t timeRemaining = 0;
  if (!readContextValue(buffer, length, offset, 0, processId) ||
      !readContextValue(buffer, length, offset, 1, initiatingDevice) ||
      !readContextValue(buffer, length, offset, 2, monitoredObject) ||
      !readContextValue(buffer, length, offset, 3, timeRemaining) ||
      offset >= length || buffer[offset++] != 0x4E) {
    return false;
  }
  notification.processId = processId;
  notification.object = BacnetObjectId{static_cast<uint16_t>(monitoredObject >> 22), monitoredObject & kObjectInstanceMask};
  bool hasPresentValue = false;
  while (offset < length && buffer[offset] != 0x4F) {
    uint32_t property = 0;
    if (!readContextValue(buffer, length, offset, 0, property)) {
      return false;
    }
    uint32_t arrayIndex = kBacnetNoArrayIndex;
    if (offset < length && (buffer[offset] >> 4) == 1 &&
        (buffer[offset] & 0x08) != 0 &&
        !readContextValue(buffer, length, offset, 1, arrayIndex)) {
      return false;
    }
    if (offset >= length || buffer[offset++] != 0x2E) {
      return false;
    }
    const BacnetPropertyId propertyId = static_cast<BacnetPropertyId>(property);
    BacnetValue value;
    if (!parseReadPropertyApplicationValue(buffer, length, offset, propertyId, value, arrayIndex) ||
        offset >= length || buffer[offset++] != 0x2F) {
      return false;
    }
    if (offset < length && (buffer[offset] >> 4) == 3 &&
        (buffer[offset] & 0x08) != 0 && (buffer[offset] & 0x07) < 5) {
      uint32_t priority = 0;
      if (!readContextValue(buffer, length, offset, 3, priority)) {
        return false;
      }
    }
    if (propertyId == BacnetPropertyId::PresentValue) {
      notification.property = propertyId;
      notification.arrayIndex = arrayIndex;
      notification.value = value;
      hasPresentValue = true;
    }
  }
  return hasPresentValue && offset + 1 == length && buffer[offset] == 0x4F;
}

bool BacnetProtocol::parseIAmResponse(const uint8_t* buffer, size_t length, BacnetIAmDeviceInfo& device) {
  if (buffer == nullptr || length < 14 || buffer[0] != kBvlcTypeBacnetIp ||
      (buffer[1] != kBvlcOriginalUnicastNpdu &&
       buffer[1] != kBvlcOriginalBroadcastNpdu) ||
      readUint16(&buffer[2]) != length) {
    return false;
  }

  size_t offset = 4;
  if (buffer[offset++] != kNpduVersion) {
    return false;
  }

  const uint8_t npduControl = buffer[offset++];
  if ((npduControl & 0x80) != 0) {
    return false;
  }

  if ((npduControl & 0x20) != 0) {
    if (!skipNpduAddress(buffer, length, offset)) {
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
      !readApplicationValue(buffer, length, offset, 2, maxApduLengthAccepted) ||
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

size_t BacnetProtocol::buildReadPropertyRequest(uint8_t* buffer,
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
  offset = writeContextUnsigned(buffer, offset, 1, static_cast<uint32_t>(request.property));
  if (request.arrayIndex != kBacnetNoArrayIndex) {
    offset = writeContextUnsigned(buffer, offset, 2, request.arrayIndex);
  }

  buffer[2] = static_cast<uint8_t>(offset >> 8);
  buffer[3] = static_cast<uint8_t>(offset);
  return offset;
}

size_t BacnetProtocol::buildWritePropertyRequest(
  uint8_t* buffer, size_t bufferSize, BacnetObjectId object, BacnetPropertyId property, const BacnetValue& value, const BacnetWritePropertyOptions& options, uint8_t invokeId) {
  if (buffer == nullptr || bufferSize < 24 || object.type > 1023 ||
      object.instance > kObjectInstanceMask ||
      (options.hasPriority && (options.priority == 0 || options.priority > 16))) {
    return 0;
  }
  size_t offset = 0;
  buffer[offset++] = kBvlcTypeBacnetIp;
  buffer[offset++] = kBvlcOriginalUnicastNpdu;
  buffer[offset++] = 0;
  buffer[offset++] = 0;
  buffer[offset++] = kNpduVersion;
  buffer[offset++] = kNpduExpectingReply;
  buffer[offset++] = kApduConfirmedRequest;
  buffer[offset++] = 0x05;
  buffer[offset++] = invokeId;
  buffer[offset++] = kServiceWriteProperty;
  offset = writeContextObjectIdentifier(buffer, offset, 0, object);
  offset = writeContextUnsigned(buffer, offset, 1, static_cast<uint32_t>(property));
  if (options.arrayIndex != kBacnetNoArrayIndex) {
    offset = writeContextUnsigned(buffer, offset, 2, options.arrayIndex);
  }
  if (offset + 2 > bufferSize) {
    return 0;
  }
  buffer[offset++] = 0x3E;
  const size_t valueLength = encodeApplicationValue(buffer + offset,
                                                    bufferSize - offset - 1,
                                                    value);
  if (valueLength == 0 || offset + valueLength + 1 > bufferSize) {
    return 0;
  }
  offset += valueLength;
  buffer[offset++] = 0x3F;
  if (options.hasPriority) {
    if (offset + 2 > bufferSize) {
      return 0;
    }
    offset = writeContextUnsigned(buffer, offset, 4, options.priority);
  }
  buffer[2] = static_cast<uint8_t>(offset >> 8);
  buffer[3] = static_cast<uint8_t>(offset);
  return offset;
}

size_t BacnetProtocol::buildWritePropertyRequest(
  uint8_t* buffer, size_t bufferSize, const BacnetPropertyRequest& request, const BacnetValue& value, uint8_t invokeId) {
  BacnetWritePropertyOptions options;
  options.arrayIndex = request.arrayIndex;
  return buildWritePropertyRequest(buffer, bufferSize, request.object, request.property, value, options, invokeId);
}

size_t BacnetProtocol::buildReadPropertyRequest(uint8_t* buffer,
                                                size_t bufferSize,
                                                BacnetObjectId object,
                                                BacnetPropertyId property,
                                                uint8_t invokeId,
                                                uint32_t arrayIndex) {
  return buildReadPropertyRequest(
    buffer, bufferSize, BacnetPropertyRequest{object, property, arrayIndex}, invokeId);
}

bool BacnetProtocol::parseReadPropertyAck(const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, const BacnetPropertyRequest& expectedRequest, BacnetValue& value) {
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
    if (expectedRequest.arrayIndex != kBacnetNoArrayIndex &&
        responseArrayIndex != expectedRequest.arrayIndex) {
      return false;
    }
  } else if (expectedRequest.arrayIndex != kBacnetNoArrayIndex) {
    return false;
  }

  if (offset >= length || buffer[offset++] != 0x3E) {
    return false;
  }

  if (!parseReadPropertyApplicationValue(buffer, length, offset, expectedRequest.property, value, responseArrayIndex)) {
    return false;
  }

  return offset < length && buffer[offset] == 0x3F;
}

bool BacnetProtocol::parseReadPropertyAck(const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, BacnetPropertyId expectedProperty, BacnetValue& value) {
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

  if (!parseReadPropertyApplicationValue(buffer, length, offset, expectedProperty, value, responseArrayIndex)) {
    return false;
  }

  return offset < length && buffer[offset] == 0x3F;
}

bool BacnetProtocol::parseReadPriorityArrayAck(
  const uint8_t* buffer,
  size_t length,
  uint8_t expectedInvokeId,
  const BacnetPropertyRequest& expectedRequest,
  BacnetPriorityArray& value) {
  value = BacnetPriorityArray{};
  if (expectedRequest.property != BacnetPropertyId::PriorityArray ||
      expectedRequest.arrayIndex != kBacnetNoArrayIndex ||
      buffer == nullptr || length < 17 ||
      buffer[0] != kBvlcTypeBacnetIp ||
      buffer[1] != kBvlcOriginalUnicastNpdu ||
      readUint16(&buffer[2]) != length) {
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
      propertyIdentifier != static_cast<uint32_t>(BacnetPropertyId::PriorityArray) ||
      offset >= length || buffer[offset++] != 0x3E) {
    return false;
  }

  for (size_t slot = 0; slot < BacnetPriorityArray::kSlotCount; ++slot) {
    if (offset >= length || buffer[offset] == 0x3F ||
        !parseReadPropertyApplicationValue(
          buffer, length, offset, BacnetPropertyId::PriorityArray, value.slots[slot])) {
      return false;
    }
    value.present[slot] = true;
  }

  return offset + 1 == length && buffer[offset] == 0x3F;
}

bool BacnetProtocol::parseReadPropertyError(const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, BacnetValue& value) {
  return parseReadPropertyError(buffer, length, expectedInvokeId, value, nullptr, nullptr);
}

bool BacnetProtocol::parseReadPropertyError(const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, BacnetValue& value, uint32_t* errorClass, uint32_t* errorCode) {
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
  char text[64] = {};
  size_t used = 0;
  if (readApplicationValue(buffer, length, offset, kApplicationTagEnumerated, parsedErrorClass) &&
      readApplicationValue(buffer, length, offset, kApplicationTagEnumerated, parsedErrorCode)) {
    if (!appendText(text, sizeof(text), used, bacnetErrorCodeName(parsedErrorCode)) ||
        !appendText(text, sizeof(text), used, " (") ||
        !appendText(text, sizeof(text), used, bacnetErrorClassName(parsedErrorClass)) ||
        !appendText(text, sizeof(text), used, "/") ||
        !appendUnsignedDecimal(text, sizeof(text), used, parsedErrorCode)) {
      return false;
    }
    if (!appendText(text, sizeof(text), used, ")")) {
      return false;
    }
    if (errorClass != nullptr) {
      *errorClass = parsedErrorClass;
    }
    if (errorCode != nullptr) {
      *errorCode = parsedErrorCode;
    }
  } else {
    if (!appendText(text, sizeof(text), used, "error")) {
      return false;
    }
    if (errorClass != nullptr) {
      *errorClass = 0;
    }
    if (errorCode != nullptr) {
      *errorCode = 0;
    }
  }

  return setTextValue(value, BacnetValueType::Error, text);
}
