// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetClient.h"

namespace {
constexpr uint8_t kBvlcTypeBacnetIp = 0x81;
constexpr uint8_t kBvlcOriginalUnicastNpdu = 0x0A;
constexpr uint8_t kBvlcOriginalBroadcastNpdu = 0x0B;
constexpr uint8_t kNpduVersion = 0x01;
constexpr uint8_t kApduUnconfirmedRequest = 0x10;
constexpr uint8_t kServiceIAm = 0x00;
constexpr uint8_t kServiceWhoIs = 0x08;
constexpr uint16_t kDeviceObjectType = 8;
constexpr uint32_t kObjectInstanceMask = 0x003FFFFF;

uint16_t readUint16(const uint8_t* buffer) {
  return (static_cast<uint16_t>(buffer[0]) << 8) | buffer[1];
}

bool readContextUnsigned(const uint8_t* buffer, size_t length, size_t& offset,
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

  if (!readContextUnsigned(buffer, length, offset, 0, objectIdentifier) ||
      (objectIdentifier >> 22) != kDeviceObjectType ||
      !readContextUnsigned(buffer, length, offset, 1,
                           maxApduLengthAccepted) ||
      !readContextUnsigned(buffer, length, offset, 2, segmentationSupported) ||
      segmentationSupported > UINT8_MAX ||
      !readContextUnsigned(buffer, length, offset, 3, vendorId) ||
      vendorId > UINT16_MAX) {
    return false;
  }

  device.deviceInstance = objectIdentifier & kObjectInstanceMask;
  device.maxApduLengthAccepted = maxApduLengthAccepted;
  device.segmentationSupported = static_cast<uint8_t>(segmentationSupported);
  device.vendorId = static_cast<uint16_t>(vendorId);
  return true;
}
