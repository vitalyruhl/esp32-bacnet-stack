// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetClient.h"

#include <cstring>

namespace {
void setReadPropertyError(BacnetValue& value, const char* text) {
  value = BacnetValue{};
  value.type = BacnetValueType::Error;
  std::strncpy(value.text, text, BacnetValue::kMaxTextLength - 1);
  value.textLength = std::strlen(value.text);
}
} // namespace

BacnetClient::BacnetClient() = default;

BacnetClient::BacnetClient(BacnetDatagramTransport& transport,
                           const BacnetMonotonicClock* clock)
    : transport_(&transport), logger_(clock) {}

bool BacnetClient::begin(uint16_t port) {
  localPort_ = port;
  running_ = transport_ != nullptr && transport_->begin(localPort_);
  if (running_) {
    logger_.info("BACnet/Client", "client started on UDP %u", static_cast<unsigned>(localPort_));
  } else {
    logger_.error("BACnet/Client", "client start failed on UDP %u", static_cast<unsigned>(localPort_));
  }
  return running_;
}

void BacnetClient::end() {
  if (running_) {
    logger_.info("BACnet/Client", "client stopped on UDP %u", static_cast<unsigned>(localPort_));
  }
  if (transport_ != nullptr) {
    transport_->end();
  }
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
bool BacnetClient::setTransport(BacnetDatagramTransport& transport) {
  if (running_) {
    return false;
  }
  transport_ = &transport;
  return true;
}
void BacnetClient::setClock(const BacnetMonotonicClock* clock) {
  logger_.setClock(clock);
}
uint32_t BacnetClient::nowMs() const {
  return logger_.clock() != nullptr ? logger_.clock()->nowMs() : 0;
}
void BacnetClient::idle() {
  if (transport_ != nullptr) {
    transport_->idle();
  }
}

bool BacnetClient::sendWhoIs(const BacnetIpEndpoint& destination) {
  uint8_t request[kWhoIsRequestSize] = {};
  const size_t requestSize = buildWhoIsRequest(request, sizeof(request));
  if (!running_ || requestSize == 0) {
    logger_.warn("BACnet/Discovery", "Who-Is send skipped to %u.%u.%u.%u:%u", destination.address[0], destination.address[1], destination.address[2], destination.address[3], static_cast<unsigned>(destination.port));
    return false;
  }
  const bool sent = transport_->send(destination, request, requestSize);
  if (sent) {
    logger_.info("BACnet/Discovery", "Who-Is sent to %u.%u.%u.%u:%u", destination.address[0], destination.address[1], destination.address[2], destination.address[3], static_cast<unsigned>(destination.port));
  } else {
    logger_.warn("BACnet/Discovery", "Who-Is send failed to %u.%u.%u.%u:%u", destination.address[0], destination.address[1], destination.address[2], destination.address[3], static_cast<unsigned>(destination.port));
  }
  return sent;
}

bool BacnetClient::sendSubscribeCov(const BacnetIpEndpoint& destination,
                                    uint32_t processId,
                                    BacnetObjectId object,
                                    uint32_t lifetimeSeconds,
                                    uint8_t invokeId) {
  uint8_t request[kMaxSubscribeCovRequestSize] = {};
  const size_t requestSize = BacnetProtocol::buildSubscribeCovRequest(
    request, sizeof(request), processId, object, lifetimeSeconds);
  if (!running_ || requestSize == 0)
    return false;
  request[8] = invokeId;
  return transport_->send(destination, request, requestSize);
}

BacnetSubscribeCovResponseKind BacnetClient::pollSubscribeCov(
  uint8_t expectedInvokeId) {
  if (!running_)
    return BacnetSubscribeCovResponseKind::None;
  uint8_t packet[kMaxDiscoveryPacketSize] = {};
  BacnetIpEndpoint source;
  const size_t bytesRead = transport_->receive(packet, sizeof(packet), source);
  if (bytesRead == 0)
    return BacnetSubscribeCovResponseKind::None;
  return BacnetProtocol::classifySubscribeCovResponse(
    packet, bytesRead, expectedInvokeId);
}

bool BacnetClient::pollCovNotification(BacnetCovNotification& notification) {
  if (!running_)
    return false;
  uint8_t packet[kMaxDiscoveryPacketSize] = {};
  BacnetIpEndpoint source;
  const size_t bytesRead = transport_->receive(packet, sizeof(packet), source);
  return bytesRead != 0 && BacnetProtocol::parseCovNotification(
                             packet, bytesRead, notification);
}

bool BacnetClient::pollIAm(BacnetIAmDevice& device) {
  if (!running_)
    return false;
  uint8_t packet[kMaxDiscoveryPacketSize] = {};
  BacnetIpEndpoint source;
  const size_t bytesRead = transport_->receive(packet, sizeof(packet), source);
  if (bytesRead == 0)
    return false;
  const bool parsed = parseIAmResponse(packet, bytesRead, device);
  if (parsed) {
    device.endpoint = source;
    logger_.info("BACnet/Discovery", "I-Am device %lu from %u.%u.%u.%u vendor %u", static_cast<unsigned long>(device.deviceInstance), source.address[0], source.address[1], source.address[2], source.address[3], static_cast<unsigned>(device.vendorId));
    if (source.isZero())
      logger_.warn("BACnet/Discovery", "I-Am device %lu has source address 0.0.0.0", static_cast<unsigned long>(device.deviceInstance));
  } else {
    logger_.debug("BACnet/Discovery", "unexpected discovery packet");
  }
  return parsed;
}

bool BacnetClient::sendReadProperty(const BacnetIpEndpoint& destination, const BacnetPropertyRequest& request, uint8_t invokeId) {
  if (destination.isZero()) {
    logger_.error("BACnet/ReadProperty", "ReadProperty %s,%lu %s skipped invoke %u: target address is 0.0.0.0", bacnetObjectTypeText(request.object.type), static_cast<unsigned long>(request.object.instance), bacnetPropertyName(request.property), static_cast<unsigned>(invokeId));
    return false;
  }
  uint8_t packet[kMaxReadPropertyRequestSize] = {};
  const size_t requestSize = buildReadPropertyRequest(packet, sizeof(packet), request, invokeId);
  if (!running_ || requestSize == 0) {
    logger_.warn("BACnet/ReadProperty", "ReadProperty %s,%lu %s skipped invoke %u", bacnetObjectTypeText(request.object.type), static_cast<unsigned long>(request.object.instance), bacnetPropertyName(request.property), static_cast<unsigned>(invokeId));
    return false;
  }
  logger_.debug("BACnet/ReadProperty", "ReadProperty %s,%lu %s queued invoke %u", bacnetObjectTypeText(request.object.type), static_cast<unsigned long>(request.object.instance), bacnetPropertyName(request.property), static_cast<unsigned>(invokeId));
  const bool sent = transport_->send(destination, packet, requestSize);
  if (sent)
    logger_.info("BACnet/ReadProperty", "ReadProperty %s,%lu %s sent invoke %u", bacnetObjectTypeText(request.object.type), static_cast<unsigned long>(request.object.instance), bacnetPropertyName(request.property), static_cast<unsigned>(invokeId));
  else
    logger_.warn("BACnet/ReadProperty", "ReadProperty %s,%lu %s send failed invoke %u", bacnetObjectTypeText(request.object.type), static_cast<unsigned long>(request.object.instance), bacnetPropertyName(request.property), static_cast<unsigned>(invokeId));
  return sent;
}

bool BacnetClient::sendReadProperty(const BacnetIpEndpoint& destination, BacnetObjectId object, BacnetPropertyId property, uint8_t invokeId, uint32_t arrayIndex) {
  return sendReadProperty(destination, BacnetPropertyRequest{object, property, arrayIndex}, invokeId);
}

bool BacnetClient::pollReadProperty(BacnetValue& value, uint8_t expectedInvokeId, const BacnetPropertyRequest& expectedRequest) {
  return pollReadPropertyStatus(value, expectedInvokeId, expectedRequest) == BacnetReadPropertyPollStatus::Ack;
}
bool BacnetClient::pollReadProperty(BacnetValue& value, uint8_t expectedInvokeId, BacnetPropertyId expectedProperty) {
  return pollReadPropertyStatus(value, expectedInvokeId, expectedProperty) == BacnetReadPropertyPollStatus::Ack;
}
void BacnetClient::logReadPropertyTimeout(uint8_t invokeId, const BacnetPropertyRequest& request) {
  logger_.warn("BACnet/ReadProperty", "ReadProperty %s,%lu %s timeout invoke %u", bacnetObjectTypeText(request.object.type), static_cast<unsigned long>(request.object.instance), bacnetPropertyName(request.property), static_cast<unsigned>(invokeId));
}

BacnetReadPropertyPollStatus BacnetClient::pollReadPropertyStatus(BacnetValue& value, uint8_t expectedInvokeId, const BacnetPropertyRequest& expectedRequest) {
  return pollReadPropertyStatus(value, expectedInvokeId, expectedRequest, nullptr, nullptr);
}
BacnetReadPropertyPollStatus BacnetClient::pollReadPropertyStatus(BacnetValue& value, uint8_t expectedInvokeId, BacnetPropertyId expectedProperty) {
  return pollReadPropertyStatus(value, expectedInvokeId, expectedProperty, nullptr, nullptr);
}

BacnetReadPropertyPollStatus BacnetClient::pollReadPropertyStatus(BacnetValue& value, uint8_t expectedInvokeId, const BacnetPropertyRequest& expectedRequest, uint32_t* errorClass, uint32_t* errorCode) {
  if (!running_)
    return BacnetReadPropertyPollStatus::None;
  uint8_t packet[kMaxDiscoveryPacketSize] = {};
  BacnetIpEndpoint source;
  const size_t bytesRead = transport_->receive(packet, sizeof(packet), source);
  if (bytesRead == 0)
    return BacnetReadPropertyPollStatus::None;
  switch (BacnetProtocol::classifyReadPropertyResponse(packet, bytesRead, expectedInvokeId)) {
    case BacnetReadPropertyResponseKind::Ack:
      if (parseReadPropertyAck(packet, bytesRead, expectedInvokeId, expectedRequest, value)) {
        logger_.info("BACnet/ReadProperty",
                     "ReadProperty %s,%lu %s = %s invoke %u",
                     bacnetObjectTypeText(expectedRequest.object.type),
                     static_cast<unsigned long>(expectedRequest.object.instance),
                     bacnetPropertyName(expectedRequest.property),
                     value.displayText(),
                     static_cast<unsigned>(expectedInvokeId));
        return BacnetReadPropertyPollStatus::Ack;
      }
      logger_.warn("BACnet/ReadProperty",
                   "ReadProperty %s,%lu %s decode error invoke %u",
                   bacnetObjectTypeText(expectedRequest.object.type),
                   static_cast<unsigned long>(expectedRequest.object.instance),
                   bacnetPropertyName(expectedRequest.property),
                   static_cast<unsigned>(expectedInvokeId));
      return BacnetReadPropertyPollStatus::DecodeError;
    case BacnetReadPropertyResponseKind::Error:
      if (parseReadPropertyError(packet, bytesRead, expectedInvokeId, value, errorClass, errorCode)) {
        logger_.warn("BACnet/ReadProperty",
                     "ReadProperty %s,%lu %s error %s invoke %u",
                     bacnetObjectTypeText(expectedRequest.object.type),
                     static_cast<unsigned long>(expectedRequest.object.instance),
                     bacnetPropertyName(expectedRequest.property),
                     value.displayText(),
                     static_cast<unsigned>(expectedInvokeId));
        return BacnetReadPropertyPollStatus::Error;
      }
      logger_.warn("BACnet/ReadProperty",
                   "ReadProperty %s,%lu %s malformed error invoke %u",
                   bacnetObjectTypeText(expectedRequest.object.type),
                   static_cast<unsigned long>(expectedRequest.object.instance),
                   bacnetPropertyName(expectedRequest.property),
                   static_cast<unsigned>(expectedInvokeId));
      return BacnetReadPropertyPollStatus::DecodeError;
    case BacnetReadPropertyResponseKind::Reject:
      setReadPropertyError(value, "reject");
      return BacnetReadPropertyPollStatus::Reject;
    case BacnetReadPropertyResponseKind::Abort:
      setReadPropertyError(value, "abort");
      return BacnetReadPropertyPollStatus::Abort;
    case BacnetReadPropertyResponseKind::Unrelated:
      return BacnetReadPropertyPollStatus::None;
  }
  return BacnetReadPropertyPollStatus::None;
}

BacnetReadPropertyPollStatus BacnetClient::pollReadPropertyStatus(BacnetValue& value, uint8_t expectedInvokeId, BacnetPropertyId expectedProperty, uint32_t* errorClass, uint32_t* errorCode) {
  if (!running_)
    return BacnetReadPropertyPollStatus::None;
  uint8_t packet[kMaxDiscoveryPacketSize] = {};
  BacnetIpEndpoint source;
  const size_t bytesRead = transport_->receive(packet, sizeof(packet), source);
  if (bytesRead == 0)
    return BacnetReadPropertyPollStatus::None;
  switch (BacnetProtocol::classifyReadPropertyResponse(packet, bytesRead, expectedInvokeId)) {
    case BacnetReadPropertyResponseKind::Ack:
      if (parseReadPropertyAck(packet, bytesRead, expectedInvokeId, expectedProperty, value)) {
        logger_.info("BACnet/ReadProperty", "ReadProperty %s = %s invoke %u", bacnetPropertyName(expectedProperty), value.displayText(), static_cast<unsigned>(expectedInvokeId));
        return BacnetReadPropertyPollStatus::Ack;
      }
      logger_.warn("BACnet/ReadProperty", "ReadProperty %s decode error invoke %u", bacnetPropertyName(expectedProperty), static_cast<unsigned>(expectedInvokeId));
      return BacnetReadPropertyPollStatus::DecodeError;
    case BacnetReadPropertyResponseKind::Error:
      if (parseReadPropertyError(packet, bytesRead, expectedInvokeId, value, errorClass, errorCode)) {
        logger_.warn("BACnet/ReadProperty", "ReadProperty %s error %s invoke %u", bacnetPropertyName(expectedProperty), value.displayText(), static_cast<unsigned>(expectedInvokeId));
        return BacnetReadPropertyPollStatus::Error;
      }
      logger_.warn("BACnet/ReadProperty",
                   "ReadProperty %s malformed error invoke %u",
                   bacnetPropertyName(expectedProperty),
                   static_cast<unsigned>(expectedInvokeId));
      return BacnetReadPropertyPollStatus::DecodeError;
    case BacnetReadPropertyResponseKind::Reject:
      setReadPropertyError(value, "reject");
      return BacnetReadPropertyPollStatus::Reject;
    case BacnetReadPropertyResponseKind::Abort:
      setReadPropertyError(value, "abort");
      return BacnetReadPropertyPollStatus::Abort;
    case BacnetReadPropertyResponseKind::Unrelated:
      return BacnetReadPropertyPollStatus::None;
  }
  return BacnetReadPropertyPollStatus::None;
}

size_t BacnetClient::buildWhoIsRequest(uint8_t* buffer, size_t bufferSize) {
  return BacnetProtocol::buildWhoIsRequest(buffer, bufferSize);
}
bool BacnetClient::parseIAmResponse(const uint8_t* buffer, size_t length, BacnetIAmDevice& device) {
  BacnetIAmDeviceInfo info;
  if (!BacnetProtocol::parseIAmResponse(buffer, length, info))
    return false;
  device.deviceInstance = info.deviceInstance;
  device.maxApduLengthAccepted = info.maxApduLengthAccepted;
  device.segmentationSupported = info.segmentationSupported;
  device.vendorId = info.vendorId;
  return true;
}
size_t BacnetClient::buildReadPropertyRequest(uint8_t* buffer, size_t bufferSize, const BacnetPropertyRequest& request, uint8_t invokeId) {
  return BacnetProtocol::buildReadPropertyRequest(buffer, bufferSize, request, invokeId);
}
size_t BacnetClient::buildReadPropertyRequest(uint8_t* buffer, size_t bufferSize, BacnetObjectId object, BacnetPropertyId property, uint8_t invokeId, uint32_t arrayIndex) {
  return BacnetProtocol::buildReadPropertyRequest(buffer, bufferSize, object, property, invokeId, arrayIndex);
}
bool BacnetClient::parseReadPropertyAck(const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, const BacnetPropertyRequest& expectedRequest, BacnetValue& value) {
  return BacnetProtocol::parseReadPropertyAck(buffer, length, expectedInvokeId, expectedRequest, value);
}
bool BacnetClient::parseReadPropertyAck(const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, BacnetPropertyId expectedProperty, BacnetValue& value) {
  return BacnetProtocol::parseReadPropertyAck(buffer, length, expectedInvokeId, expectedProperty, value);
}
bool BacnetClient::parseReadPropertyError(const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, BacnetValue& value) {
  return BacnetProtocol::parseReadPropertyError(buffer, length, expectedInvokeId, value);
}
bool BacnetClient::parseReadPropertyError(const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, BacnetValue& value, uint32_t* errorClass, uint32_t* errorCode) {
  return BacnetProtocol::parseReadPropertyError(buffer, length, expectedInvokeId, value, errorClass, errorCode);
}
