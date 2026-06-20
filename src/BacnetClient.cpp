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
constexpr uint8_t kApduUnconfirmedRequest = 0x10;
constexpr uint8_t kServiceIAm = 0x00;
constexpr uint8_t kServiceWhoIs = 0x08;
constexpr uint8_t kServiceReadProperty = 0x0C;
constexpr uint16_t kDeviceObjectType = 8;
constexpr uint32_t kObjectInstanceMask = 0x003FFFFF;
constexpr uint8_t kApplicationTagUnsigned = 2;
constexpr uint8_t kApplicationTagReal = 4;
constexpr uint8_t kApplicationTagCharacterString = 7;
constexpr uint8_t kApplicationTagEnumerated = 9;
constexpr uint8_t kApplicationTagObjectIdentifier = 12;

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

bool parseApplicationUnsigned(const uint8_t* buffer, size_t length,
                              size_t& offset, uint8_t expectedTag,
                              BacnetValue& value) {
  uint32_t parsedValue = 0;
  if (!readApplicationValue(buffer, length, offset, expectedTag, parsedValue)) {
    return false;
  }

  char text[BacnetValue::kMaxTextLength] = {};
  std::snprintf(text, sizeof(text), "%lu",
                static_cast<unsigned long>(parsedValue));
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

  char text[BacnetValue::kMaxTextLength] = {};
  std::snprintf(text, sizeof(text), "%.3f", static_cast<double>(parsedValue));
  return copyTextValue(value, text);
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
  offset += stringLength;
  return true;
}

bool parseReadPropertyApplicationValue(const uint8_t* buffer, size_t length,
                                       size_t& offset,
                                       BacnetPropertyId expectedProperty,
                                       BacnetValue& value) {
  if (offset >= length) {
    return false;
  }

  if (expectedProperty == BacnetPropertyId::PresentValue) {
    const uint8_t tagNumber = buffer[offset] >> 4;
    if (tagNumber == kApplicationTagUnsigned) {
      return parseApplicationUnsigned(buffer, length, offset,
                                      kApplicationTagUnsigned, value);
    }
    if (tagNumber == kApplicationTagEnumerated) {
      return parseApplicationUnsigned(buffer, length, offset,
                                      kApplicationTagEnumerated, value);
    }
    if (tagNumber == kApplicationTagReal) {
      return parseApplicationReal(buffer, length, offset, value);
    }
  }

  return parseApplicationCharacterString(buffer, length, offset, value);
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
}  // namespace

bool BacnetClient::begin(uint16_t localPort) {
  localPort_ = localPort;
  running_ = udp_.begin(localPort_) == 1;
  return running_;
}

void BacnetClient::end() {
  udp_.stop();
  running_ = false;
}

bool BacnetClient::isRunning() const {
  return running_;
}

uint16_t BacnetClient::localPort() const {
  return localPort_;
}

bool BacnetClient::sendWhoIs(IPAddress address, uint16_t port) {
  uint8_t request[kWhoIsRequestSize] = {};
  const size_t requestSize = buildWhoIsRequest(request, sizeof(request));

  if (!running_ || requestSize == 0) {
    return false;
  }

  if (udp_.beginPacket(address, port) != 1) {
    return false;
  }

  const size_t bytesWritten = udp_.write(request, requestSize);
  return bytesWritten == requestSize && udp_.endPacket() == 1;
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

  return parseIAmResponse(packet, static_cast<size_t>(bytesRead), device);
}

bool BacnetClient::sendReadProperty(IPAddress address, BacnetObjectId object,
                                    BacnetPropertyId property, uint8_t invokeId,
                                    uint16_t port) {
  uint8_t request[kMaxReadPropertyRequestSize] = {};
  const size_t requestSize =
      buildReadPropertyRequest(request, sizeof(request), object, property,
                               invokeId);

  if (!running_ || requestSize == 0) {
    return false;
  }

  if (udp_.beginPacket(address, port) != 1) {
    return false;
  }

  const size_t bytesWritten = udp_.write(request, requestSize);
  return bytesWritten == requestSize && udp_.endPacket() == 1;
}

bool BacnetClient::pollReadProperty(BacnetValue& value,
                                    uint8_t expectedInvokeId,
                                    BacnetPropertyId expectedProperty) {
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

  return parseReadPropertyAck(packet, static_cast<size_t>(bytesRead),
                              expectedInvokeId, expectedProperty, value);
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
                                              BacnetObjectId object,
                                              BacnetPropertyId property,
                                              uint8_t invokeId) {
  if (buffer == nullptr || bufferSize < kMaxReadPropertyRequestSize ||
      object.instance > kObjectInstanceMask) {
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

  offset = writeContextUnsigned(buffer, offset, 0, encodeObjectId(object));
  offset = writeContextUnsigned(buffer, offset, 1,
                                static_cast<uint32_t>(property));

  buffer[2] = static_cast<uint8_t>(offset >> 8);
  buffer[3] = static_cast<uint8_t>(offset);
  return offset;
}

bool BacnetClient::parseReadPropertyAck(const uint8_t* buffer, size_t length,
                                        uint8_t expectedInvokeId,
                                        BacnetPropertyId expectedProperty,
                                        BacnetValue& value) {
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

  if (offset >= length || buffer[offset++] != 0x3E) {
    return false;
  }

  if (!parseReadPropertyApplicationValue(buffer, length, offset,
                                         expectedProperty, value)) {
    return false;
  }

  return offset < length && buffer[offset] == 0x3F;
}
