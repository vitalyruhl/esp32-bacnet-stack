// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetClient.h"
#include "BacnetFeatureGates.h"

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
  queuedCovNotificationCount_ = 0;
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
  queuedCovNotificationCount_ = 0;
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
                                    uint8_t invokeId,
                                    bool issueConfirmedNotifications) {
  uint8_t request[kMaxSubscribeCovRequestSize] = {};
  const size_t requestSize = BacnetProtocol::buildSubscribeCovRequest(
    request, sizeof(request), processId, object, lifetimeSeconds, issueConfirmedNotifications);
  if (!running_ || requestSize == 0)
    return false;
  request[8] = invokeId;
  return transport_->send(destination, request, requestSize);
}

bool BacnetClient::sendSubscribeCovProperty(const BacnetIpEndpoint& destination,
                                            uint32_t processId,
                                            BacnetObjectId object,
                                            BacnetPropertyId property,
                                            uint32_t lifetimeSeconds,
                                            uint8_t invokeId,
                                            bool issueConfirmedNotifications,
                                            uint32_t arrayIndex,
                                            bool hasCovIncrement,
                                            float covIncrement) {
  uint8_t request[kMaxSubscribeCovRequestSize] = {};
  const size_t requestSize = BacnetProtocol::buildSubscribeCovPropertyRequest(
    request, sizeof(request), processId, object, property, lifetimeSeconds, issueConfirmedNotifications, arrayIndex, hasCovIncrement, covIncrement);
  if (!running_ || requestSize == 0) {
    return false;
  }
  request[8] = invokeId;
  return transport_->send(destination, request, requestSize);
}

BacnetSubscribeCovResponseKind BacnetClient::pollSubscribeCov(
  uint8_t expectedInvokeId,
  uint8_t* rejectReason,
  const BacnetIpEndpoint* expectedPeer) {
  if (!running_)
    return BacnetSubscribeCovResponseKind::None;
  uint8_t packet[kMaxDiscoveryPacketSize] = {};
  BacnetIpEndpoint source;
  const size_t bytesRead = transport_->receive(packet, sizeof(packet), source);
  if (bytesRead == 0)
    return BacnetSubscribeCovResponseKind::None;
  if (queueCovNotification(packet, bytesRead, source)) {
    return BacnetSubscribeCovResponseKind::None;
  }
  if (expectedPeer != nullptr && !endpointEquals(source, *expectedPeer)) {
    return BacnetSubscribeCovResponseKind::None;
  }
  return BacnetProtocol::classifySubscribeCovResponse(
    packet, bytesRead, expectedInvokeId, rejectReason);
}

BacnetSubscribeCovResponseKind BacnetClient::pollSubscribeCovProperty(
  uint8_t expectedInvokeId,
  uint8_t* rejectReason,
  const BacnetIpEndpoint* expectedPeer) {
  if (!running_) {
    return BacnetSubscribeCovResponseKind::None;
  }
  uint8_t packet[kMaxDiscoveryPacketSize] = {};
  BacnetIpEndpoint source;
  const size_t bytesRead = transport_->receive(packet, sizeof(packet), source);
  if (bytesRead == 0) {
    return BacnetSubscribeCovResponseKind::None;
  }
  if (queueCovNotification(packet, bytesRead, source)) {
    return BacnetSubscribeCovResponseKind::None;
  }
  if (expectedPeer != nullptr && !endpointEquals(source, *expectedPeer)) {
    return BacnetSubscribeCovResponseKind::None;
  }
  return BacnetProtocol::classifySubscribeCovResponse(
    packet, bytesRead, expectedInvokeId, rejectReason, 0x1CU);
}

bool BacnetClient::pollCovNotification(BacnetCovNotification& notification,
                                       bool acknowledgeConfirmed) {
  if (!running_)
    return false;
  if (takeQueuedCovNotification(notification)) {
    return !acknowledgeConfirmed || acknowledgeCovNotification(notification);
  }
  uint8_t packet[kMaxDiscoveryPacketSize] = {};
  BacnetIpEndpoint source;
  const size_t bytesRead = transport_->receive(packet, sizeof(packet), source);
  if (bytesRead == 0) {
    return false;
  }
  BacnetCovNotification received;
  if (!BacnetProtocol::parseCovNotification(packet, bytesRead, received)) {
    return false;
  }
  received.source = source;
  if (acknowledgeConfirmed && !acknowledgeCovNotification(received)) {
    return false;
  }
  notification = received;
  return true;
}

bool BacnetClient::queueCovNotification(const uint8_t* packet,
                                        size_t length,
                                        const BacnetIpEndpoint& source) {
  BacnetCovNotification notification;
  if (!BacnetProtocol::parseCovNotification(packet, length, notification)) {
    return false;
  }
  notification.source = source;
  if (queuedCovNotificationCount_ == kMaxQueuedCovNotifications) {
    for (size_t index = 1; index < queuedCovNotificationCount_; ++index) {
      queuedCovNotifications_[index - 1] = queuedCovNotifications_[index];
    }
    --queuedCovNotificationCount_;
  }
  queuedCovNotifications_[queuedCovNotificationCount_++] = notification;
  return true;
}

bool BacnetClient::takeQueuedCovNotification(BacnetCovNotification& notification) {
  if (queuedCovNotificationCount_ == 0) {
    return false;
  }
  notification = queuedCovNotifications_[0];
  for (size_t index = 1; index < queuedCovNotificationCount_; ++index) {
    queuedCovNotifications_[index - 1] = queuedCovNotifications_[index];
  }
  --queuedCovNotificationCount_;
  return true;
}

bool BacnetClient::acknowledgeCovNotification(
  const BacnetCovNotification& notification) {
  if (!notification.confirmed) {
    return true;
  }
  uint8_t acknowledgement[16] = {};
  const size_t acknowledgementSize = BacnetProtocol::buildSimpleAckResponse(
    acknowledgement, sizeof(acknowledgement), notification.invokeId, 0x01U);
  return acknowledgementSize != 0 &&
         transport_->send(notification.source, acknowledgement, acknowledgementSize);
}

bool BacnetClient::endpointEquals(const BacnetIpEndpoint& left,
                                  const BacnetIpEndpoint& right) {
  return left.port == right.port && left.address[0] == right.address[0] &&
         left.address[1] == right.address[1] && left.address[2] == right.address[2] &&
         left.address[3] == right.address[3];
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

BacnetWritePropertyPollStatus BacnetClient::sendWriteProperty(
  const BacnetIpEndpoint& destination, const BacnetPropertyRequest& request, const BacnetValue& value, uint8_t invokeId) {
#if !ESP_BACNET_ENABLE_WRITE_PROPERTY
  (void)destination;
  (void)request;
  (void)value;
  (void)invokeId;
  logger_.warn("BACnet/WriteProperty",
               "WriteProperty rejected: ESP_BACNET_ENABLE_WRITE_PROPERTY=0");
  return BacnetWritePropertyPollStatus::Disabled;
#else
  if (!running_ || destination.isZero()) {
    return BacnetWritePropertyPollStatus::SendFailed;
  }
  if (request.object.type > 1023 || request.object.instance > 0x003FFFFFUL) {
    return BacnetWritePropertyPollStatus::InvalidArgument;
  }
  uint8_t packet[BacnetProtocol::kMaxWritePropertyRequestSize] = {};
  const size_t packetSize = BacnetProtocol::buildWritePropertyRequest(
    packet, sizeof(packet), request, value, invokeId);
  if (packetSize == 0) {
    return BacnetWritePropertyPollStatus::UnsupportedValue;
  }
  if (!transport_->send(destination, packet, packetSize)) {
    return BacnetWritePropertyPollStatus::SendFailed;
  }
  logger_.info("BACnet/WriteProperty",
               "WriteProperty %s,%lu %s sent invoke %u",
               bacnetObjectTypeText(request.object.type),
               static_cast<unsigned long>(request.object.instance),
               bacnetPropertyName(request.property),
               static_cast<unsigned>(invokeId));
  return BacnetWritePropertyPollStatus::None;
#endif
}

BacnetWritePropertyPollStatus BacnetClient::sendWriteProperty(
  const BacnetIpEndpoint& destination, BacnetObjectId object, BacnetPropertyId property, const BacnetValue& value, const BacnetWritePropertyOptions& options, uint8_t invokeId) {
#if !ESP_BACNET_ENABLE_WRITE_PROPERTY
  (void)destination;
  (void)object;
  (void)property;
  (void)value;
  (void)options;
  (void)invokeId;
  return BacnetWritePropertyPollStatus::Disabled;
#else
  if (!bacnetWritePropertyEnabled(options.hasPriority)) {
    return BacnetWritePropertyPollStatus::Disabled;
  }
  if (!running_ || destination.isZero()) {
    return BacnetWritePropertyPollStatus::SendFailed;
  }
  if (object.type > 1023 || object.instance > 0x003FFFFFUL ||
      (options.hasPriority && (options.priority == 0 || options.priority > 16))) {
    return BacnetWritePropertyPollStatus::InvalidArgument;
  }
  uint8_t packet[BacnetProtocol::kMaxWritePropertyRequestSize] = {};
  const size_t packetSize = BacnetProtocol::buildWritePropertyRequest(
    packet, sizeof(packet), object, property, value, options, invokeId);
  if (packetSize == 0) {
    return BacnetWritePropertyPollStatus::UnsupportedValue;
  }
  return transport_->send(destination, packet, packetSize)
           ? BacnetWritePropertyPollStatus::None
           : BacnetWritePropertyPollStatus::SendFailed;
#endif
}

BacnetWritePropertyPollStatus BacnetClient::pollWriteProperty(
  uint8_t expectedInvokeId) {
#if !ESP_BACNET_ENABLE_WRITE_PROPERTY
  (void)expectedInvokeId;
  return BacnetWritePropertyPollStatus::Disabled;
#else
  if (!running_) {
    return BacnetWritePropertyPollStatus::SendFailed;
  }
  uint8_t packet[kMaxDiscoveryPacketSize] = {};
  BacnetIpEndpoint source;
  const size_t bytesRead = transport_->receive(packet, sizeof(packet), source);
  if (bytesRead == 0) {
    return BacnetWritePropertyPollStatus::None;
  }
  switch (BacnetProtocol::classifyWritePropertyResponse(
    packet, bytesRead, expectedInvokeId)) {
    case BacnetWritePropertyResponseKind::Ack:
      return BacnetWritePropertyPollStatus::Ack;
    case BacnetWritePropertyResponseKind::Error:
      if (bytesRead >= 13 && packet[9] == 0x91 && packet[10] == 0x02 &&
          packet[11] == 0x91 && packet[12] == 40) {
        return BacnetWritePropertyPollStatus::NotCommandable;
      }
      return BacnetWritePropertyPollStatus::Error;
    case BacnetWritePropertyResponseKind::Reject:
      return BacnetWritePropertyPollStatus::Reject;
    case BacnetWritePropertyResponseKind::Abort:
      return BacnetWritePropertyPollStatus::Abort;
    case BacnetWritePropertyResponseKind::Unrelated:
      return BacnetWritePropertyPollStatus::None;
  }
  return BacnetWritePropertyPollStatus::None;
#endif
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

BacnetReadPropertyPollStatus BacnetClient::pollReadPriorityArrayStatus(
  BacnetPriorityArray& value,
  uint8_t expectedInvokeId,
  const BacnetPropertyRequest& expectedRequest,
  uint32_t* errorClass,
  uint32_t* errorCode) {
  value = BacnetPriorityArray{};
  if (!running_) {
    return BacnetReadPropertyPollStatus::None;
  }
  uint8_t packet[kMaxDiscoveryPacketSize] = {};
  BacnetIpEndpoint source;
  const size_t bytesRead = transport_->receive(packet, sizeof(packet), source);
  if (bytesRead == 0) {
    return BacnetReadPropertyPollStatus::None;
  }

  switch (BacnetProtocol::classifyReadPropertyResponse(packet, bytesRead, expectedInvokeId)) {
    case BacnetReadPropertyResponseKind::Ack:
      if (BacnetProtocol::parseReadPriorityArrayAck(
            packet, bytesRead, expectedInvokeId, expectedRequest, value)) {
        logger_.info("BACnet/ReadProperty",
                     "ReadProperty %s,%lu %s = 16 slots invoke %u",
                     bacnetObjectTypeText(expectedRequest.object.type),
                     static_cast<unsigned long>(expectedRequest.object.instance),
                     bacnetPropertyName(expectedRequest.property),
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
    case BacnetReadPropertyResponseKind::Error: {
      BacnetValue error;
      if (parseReadPropertyError(packet, bytesRead, expectedInvokeId, error, errorClass, errorCode)) {
        logger_.warn("BACnet/ReadProperty",
                     "ReadProperty %s,%lu %s error %s invoke %u",
                     bacnetObjectTypeText(expectedRequest.object.type),
                     static_cast<unsigned long>(expectedRequest.object.instance),
                     bacnetPropertyName(expectedRequest.property),
                     error.displayText(),
                     static_cast<unsigned>(expectedInvokeId));
        return BacnetReadPropertyPollStatus::Error;
      }
      return BacnetReadPropertyPollStatus::DecodeError;
    }
    case BacnetReadPropertyResponseKind::Reject:
      return BacnetReadPropertyPollStatus::Reject;
    case BacnetReadPropertyResponseKind::Abort:
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
size_t BacnetClient::buildWritePropertyRequest(
  uint8_t* buffer, size_t bufferSize, const BacnetPropertyRequest& request, const BacnetValue& value, uint8_t invokeId) {
  return BacnetProtocol::buildWritePropertyRequest(buffer, bufferSize, request, value, invokeId);
}
size_t BacnetClient::buildWritePropertyRequest(
  uint8_t* buffer, size_t bufferSize, BacnetObjectId object, BacnetPropertyId property, const BacnetValue& value, const BacnetWritePropertyOptions& options, uint8_t invokeId) {
  return BacnetProtocol::buildWritePropertyRequest(buffer, bufferSize, object, property, value, options, invokeId);
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
