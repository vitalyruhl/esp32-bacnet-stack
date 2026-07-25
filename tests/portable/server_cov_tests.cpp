// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetServer.h"

#include <cstring>
#include <limits>

namespace {

class TestClock final : public BacnetMonotonicClock {
public:
  uint32_t nowMs() const override {
    return now;
  }
  uint32_t now = 0;
};

class TestTransport final : public BacnetDatagramTransport {
public:
  bool begin(uint16_t) override {
    return true;
  }
  void end() override {}
  bool send(const BacnetIpEndpoint& destination, const uint8_t* data, size_t length) override {
    ++sendCalls;
    if (data == nullptr || length > sizeof(lastSent)) {
      return false;
    }
    lastDestination = destination;
    std::memcpy(lastSent, data, length);
    lastSentLength = length;
    return sendResult;
  }
  size_t receive(uint8_t* data, size_t capacity, BacnetIpEndpoint& source) override {
    if (incomingLength == 0 || incomingLength > capacity) {
      return 0;
    }
    std::memcpy(data, incoming, incomingLength);
    source = incomingSource;
    const size_t result = incomingLength;
    incomingLength = 0;
    return result;
  }
  void idle() override {}
  void queue(const uint8_t* data, size_t length, const BacnetIpEndpoint& source) {
    if (data == nullptr || length > sizeof(incoming)) {
      incomingLength = 0;
      return;
    }
    std::memcpy(incoming, data, length);
    incomingLength = length;
    incomingSource = source;
  }

  uint8_t incoming[BacnetServer::kMaxDatagramSize] = {};
  size_t incomingLength = 0;
  BacnetIpEndpoint incomingSource;
  uint8_t lastSent[BacnetServer::kMaxDatagramSize] = {};
  size_t lastSentLength = 0;
  BacnetIpEndpoint lastDestination;
  uint32_t sendCalls = 0;
  bool sendResult = true;
};

bool simpleAckIsFor(const TestTransport& transport, uint8_t invoke, uint8_t service) {
  return BacnetProtocol::isSimpleAck(transport.lastSent, transport.lastSentLength, invoke, service);
}

bool testSubscribeCovLifecycle() {
  TestTransport transport;
  TestClock clock;
  BacnetServer server(transport);
  server.setClock(&clock);
  BacnetServerAnalogInput inputs[] = {{0, "Input", 1.0F, BacnetEngineeringUnits::Percent}};
  BacnetServerDevice device;
  device.deviceInstance = 100;
  if (!server.setAnalogInputs(inputs, 1) || !server.begin(device)) {
    return false;
  }
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::AnalogInput), 0};
  const BacnetIpEndpoint peer(192, 0, 2, 7, 47808);
  uint8_t request[BacnetProtocol::kMaxSubscribeCovRequestSize] = {};
  const size_t requestSize = BacnetProtocol::buildSubscribeCovRequest(
    request, sizeof(request), 7, object, 30);
  if (requestSize == 0) {
    return false;
  }
  request[8] = 11;
  transport.queue(request, requestSize, peer);
  if (server.poll() != BacnetServerPollResult::SubscribeCovAckSent ||
      !simpleAckIsFor(transport, 11, 5) || server.covSubscriptionCount() != 1) {
    return false;
  }
  if (server.poll() != BacnetServerPollResult::CovNotificationSent) {
    return false;
  }
  BacnetCovNotification notification;
  if (!BacnetProtocol::parseCovNotification(transport.lastSent, transport.lastSentLength, notification) ||
      notification.processId != 7 || notification.object.type != object.type ||
      notification.object.instance != object.instance ||
      notification.propertyCount != 2 ||
      notification.properties[0].property != BacnetPropertyId::PresentValue ||
      notification.properties[0].value.type != BacnetValueType::Real ||
      notification.properties[0].value.realValue != 1.0F ||
      notification.properties[1].property != BacnetPropertyId::StatusFlags ||
      notification.properties[1].value.type != BacnetValueType::BitString) {
    return false;
  }
  inputs[0].presentValue = 2.0F;
  if (server.poll() != BacnetServerPollResult::CovNotificationSent ||
      !BacnetProtocol::parseCovNotification(transport.lastSent, transport.lastSentLength, notification) || notification.propertyCount != 2 ||
      notification.value.realValue != 2.0F ||
      server.poll() != BacnetServerPollResult::NoDatagram) {
    return false;
  }

  const size_t cancelSize = BacnetProtocol::buildSubscribeCovRequest(
    request, sizeof(request), 7, object, 0);
  if (cancelSize == 0) {
    return false;
  }
  request[8] = 12;
  transport.queue(request, cancelSize, peer);
  if (server.poll() != BacnetServerPollResult::SubscribeCovAckSent ||
      !simpleAckIsFor(transport, 12, 5) || server.covSubscriptionCount() != 0) {
    return false;
  }
  const size_t shortLifetimeSize = BacnetProtocol::buildSubscribeCovRequest(
    request, sizeof(request), 8, object, 1);
  request[8] = 13;
  transport.queue(request, shortLifetimeSize, peer);
  if (server.poll() != BacnetServerPollResult::SubscribeCovAckSent ||
      server.covSubscriptionCount() != 1) {
    return false;
  }
  clock.now = 1000;
  return server.poll() == BacnetServerPollResult::NoDatagram &&
         server.covSubscriptionCount() == 0;
}

bool testSubscribeCovPropertyAndConfirmedAck() {
  TestTransport transport;
  TestClock clock;
  BacnetServer server(transport);
  server.setClock(&clock);
  BacnetServerBinaryInput inputs[] = {{0, "Button", false}};
  BacnetServerDevice device;
  device.deviceInstance = 101;
  if (!server.setBinaryInputs(inputs, 1) || !server.begin(device)) {
    return false;
  }
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::BinaryInput), 0};
  const BacnetIpEndpoint peer(192, 0, 2, 8, 47809);
  uint8_t request[64] = {};
  size_t requestSize = BacnetProtocol::buildSubscribeCovPropertyRequest(
    request, sizeof(request), 9, object, BacnetPropertyId::PresentValue, 30, true);
  if (requestSize == 0) {
    return false;
  }
  request[8] = 13;
  transport.queue(request, requestSize, peer);
  if (server.poll() != BacnetServerPollResult::SubscribeCovAckSent ||
      !simpleAckIsFor(transport, 13, 0x1C)) {
    return false;
  }
  if (server.poll() != BacnetServerPollResult::CovNotificationSent ||
      transport.lastSentLength < 10 || transport.lastSent[6] != 0x00 ||
      transport.lastSent[9] != 0x01) {
    return false;
  }
  BacnetServerCovSubscription subscription;
  if (!server.covSubscriptionAt(0, subscription) ||
      subscription.state != BacnetServerCovSubscriptionState::AwaitingConfirmedAck) {
    return false;
  }
  uint8_t ack[16] = {};
  const size_t ackSize = BacnetProtocol::buildSimpleAckResponse(
    ack, sizeof(ack), subscription.pendingInvokeId, 0x01);
  transport.queue(ack, ackSize, peer);
  if (server.poll() != BacnetServerPollResult::Ignored ||
      !server.covSubscriptionAt(0, subscription) ||
      subscription.state != BacnetServerCovSubscriptionState::Active ||
      subscription.lastAckMs != clock.now) {
    return false;
  }
  requestSize = BacnetProtocol::buildSubscribeCovPropertyRequest(
    request, sizeof(request), 10, object, BacnetPropertyId::StatusFlags, 30);
  if (requestSize == 0) {
    return false;
  }
  request[8] = 14;
  transport.queue(request, requestSize, peer);
  if (server.poll() != BacnetServerPollResult::SubscribeCovAckSent ||
      !simpleAckIsFor(transport, 14, 0x1C) ||
      server.poll() != BacnetServerPollResult::CovNotificationSent) {
    return false;
  }
  BacnetCovNotification statusNotification;
  if (!BacnetProtocol::parseCovNotification(transport.lastSent,
                                            transport.lastSentLength,
                                            statusNotification) ||
      statusNotification.property != BacnetPropertyId::StatusFlags ||
      statusNotification.value.type != BacnetValueType::BitString) {
    return false;
  }
  inputs[0].presentValue = true;
  if (server.poll() != BacnetServerPollResult::CovNotificationSent ||
      !server.covSubscriptionAt(0, subscription)) {
    return false;
  }
  const uint8_t error[] = {0x81, 0x0A, 0x00, 0x09, 0x01, 0x00, 0x50, subscription.pendingInvokeId, 0x01};
  transport.queue(error, sizeof(error), peer);
  if (server.poll() != BacnetServerPollResult::Ignored ||
      !server.covSubscriptionAt(0, subscription) ||
      subscription.state != BacnetServerCovSubscriptionState::Active) {
    return false;
  }
  inputs[0].presentValue = false;
  if (server.poll() != BacnetServerPollResult::CovNotificationSent ||
      !server.covSubscriptionAt(0, subscription)) {
    return false;
  }
  const uint8_t reject[] = {0x81, 0x0A, 0x00, 0x09, 0x01, 0x00, 0x60, subscription.pendingInvokeId, 0x04};
  transport.queue(reject, sizeof(reject), peer);
  if (server.poll() != BacnetServerPollResult::Ignored) {
    return false;
  }
  inputs[0].presentValue = true;
  if (server.poll() != BacnetServerPollResult::CovNotificationSent ||
      !server.covSubscriptionAt(0, subscription)) {
    return false;
  }
  const uint8_t abort[] = {0x81, 0x0A, 0x00, 0x08, 0x01, 0x00, 0x70, subscription.pendingInvokeId};
  transport.queue(abort, sizeof(abort), peer);
  if (server.poll() != BacnetServerPollResult::Ignored) {
    return false;
  }

  inputs[0].presentValue = false;
  if (server.poll() != BacnetServerPollResult::CovNotificationSent) {
    return false;
  }
  for (uint32_t retry = 0; retry < device.numberOfApduRetries; ++retry) {
    clock.now += device.apduTimeout;
    if (server.poll() != BacnetServerPollResult::CovNotificationSent) {
      return false;
    }
  }
  clock.now += device.apduTimeout;
  if (server.poll() != BacnetServerPollResult::NoDatagram ||
      !server.covSubscriptionAt(0, subscription) ||
      subscription.state != BacnetServerCovSubscriptionState::Active) {
    return false;
  }
  return true;
}

bool testSubscribeCovPropertyIncrement() {
  TestTransport transport;
  BacnetServer server(transport);
  BacnetServerAnalogInput inputs[] = {{0, "Light", 10.0F, BacnetEngineeringUnits::Percent}};
  BacnetServerDevice device;
  device.deviceInstance = 104;
  if (!server.setAnalogInputs(inputs, 1) || !server.begin(device)) {
    return false;
  }
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::AnalogInput), 0};
  const BacnetIpEndpoint peer(192, 0, 2, 10, 47810);
  uint8_t request[BacnetProtocol::kMaxSubscribeCovRequestSize] = {};
  const size_t requestSize = BacnetProtocol::buildSubscribeCovPropertyRequest(
    request, sizeof(request), 14, object, BacnetPropertyId::PresentValue, 30, false, kBacnetNoArrayIndex, true, 0.5F);
  if (requestSize == 0) {
    return false;
  }
  request[8] = 16;
  transport.queue(request, requestSize, peer);
  if (server.poll() != BacnetServerPollResult::SubscribeCovAckSent ||
      server.poll() != BacnetServerPollResult::CovNotificationSent) {
    return false;
  }
  inputs[0].presentValue = 10.25F;
  if (server.poll() != BacnetServerPollResult::NoDatagram) {
    return false;
  }
  inputs[0].presentValue = 10.5F;
  return server.poll() == BacnetServerPollResult::CovNotificationSent;
}

bool testBinaryOutputCovTracksEffectivePresentValue() {
  TestTransport transport;
  BacnetServer server(transport);
  BacnetBinaryOutput output;
  if (output.configure(0, "LED") != BacnetObjectConfigurationStatus::Ok ||
      server.addObject(output) != BacnetObjectConfigurationStatus::Ok) {
    return false;
  }
  BacnetServerDevice device;
  device.deviceInstance = 105;
  if (!server.begin(device)) {
    return false;
  }
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::BinaryOutput), 0};
  const BacnetIpEndpoint peer(192, 0, 2, 11, 47811);
  uint8_t request[BacnetProtocol::kMaxSubscribeCovRequestSize] = {};
  const size_t requestSize = BacnetProtocol::buildSubscribeCovPropertyRequest(
    request, sizeof(request), 15, object, BacnetPropertyId::PresentValue, 30);
  if (requestSize == 0) {
    return false;
  }
  request[8] = 17;
  transport.queue(request, requestSize, peer);
  if (server.poll() != BacnetServerPollResult::SubscribeCovAckSent ||
      server.poll() != BacnetServerPollResult::CovNotificationSent ||
      !output.writeValue(true, 8) ||
      server.poll() != BacnetServerPollResult::CovNotificationSent ||
      !output.writeValue(false, 9) ||
      server.poll() != BacnetServerPollResult::NoDatagram ||
      !output.writeValue(false, 7)) {
    return false;
  }
  return server.poll() == BacnetServerPollResult::CovNotificationSent;
}

bool testSubscriptionCapacityAndErrors() {
  TestTransport transport;
  BacnetServer server(transport);
  BacnetServerBinaryInput inputs[] = {{0, "Button", false}};
  BacnetServerDevice device;
  device.deviceInstance = 102;
  if (!server.setBinaryInputs(inputs, 1) || !server.begin(device)) {
    return false;
  }
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::BinaryInput), 0};
  uint8_t request[BacnetProtocol::kMaxSubscribeCovRequestSize] = {};
  for (uint32_t index = 0; index < BacnetServer::kMaxCovSubscriptions; ++index) {
    const size_t requestSize = BacnetProtocol::buildSubscribeCovRequest(
      request, sizeof(request), index + 1U, object, 30);
    request[8] = static_cast<uint8_t>(index + 1U);
    transport.queue(request, requestSize, BacnetIpEndpoint(192, 0, 2, static_cast<uint8_t>(20U + index), 47808));
    if (server.poll() != BacnetServerPollResult::SubscribeCovAckSent) {
      return false;
    }
  }
  const size_t overflowSize = BacnetProtocol::buildSubscribeCovRequest(
    request, sizeof(request), 99, object, 30);
  request[8] = 99;
  transport.queue(request, overflowSize, BacnetIpEndpoint(192, 0, 2, 99, 47808));
  return server.poll() == BacnetServerPollResult::SubscribeCovErrorSent &&
         server.covSubscriptionCount() == BacnetServer::kMaxCovSubscriptions;
}

bool testBinaryOutputCovFollowsEffectiveValue() {
  TestTransport transport;
  TestClock clock;
  BacnetServer server(transport);
  server.setClock(&clock);
  BacnetServerBinaryOutput outputs[] = {{0, "Output"}};
  outputs[0].priority.relinquishDefault = false;
  BacnetServerDevice device;
  device.deviceInstance = 103;
  if (!server.setBinaryOutputs(outputs, 1) || !server.begin(device)) {
    return false;
  }
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::BinaryOutput), 0};
  const BacnetIpEndpoint peer(192, 0, 2, 30, 47808);
  uint8_t subscribe[BacnetProtocol::kMaxSubscribeCovRequestSize] = {};
  size_t subscribeSize = BacnetProtocol::buildSubscribeCovRequest(
    subscribe, sizeof(subscribe), 30, object, 30);
  subscribe[8] = 30;
  transport.queue(subscribe, subscribeSize, peer);
  if (server.poll() != BacnetServerPollResult::SubscribeCovAckSent ||
      server.poll() != BacnetServerPollResult::CovNotificationSent) {
    return false;
  }
  BacnetWritePropertyOptions priority8;
  priority8.hasPriority = true;
  priority8.priority = 8;
  BacnetValue inactive;
  inactive.type = BacnetValueType::Enumerated;
  inactive.unsignedValue = 0;
  BacnetValue active;
  active.type = BacnetValueType::Enumerated;
  active.unsignedValue = 1;
  uint8_t write[BacnetProtocol::kMaxWritePropertyRequestSize] = {};
  size_t writeSize = BacnetProtocol::buildWritePropertyRequest(
    write, sizeof(write), object, BacnetPropertyId::PresentValue, inactive, priority8, 31);
  transport.queue(write, writeSize, peer);
  if (server.poll() != BacnetServerPollResult::WritePropertyAckSent ||
      server.poll() != BacnetServerPollResult::NoDatagram) {
    return false;
  }
  writeSize = BacnetProtocol::buildWritePropertyRequest(
    write, sizeof(write), object, BacnetPropertyId::PresentValue, active, priority8, 32);
  transport.queue(write, writeSize, peer);
  if (server.poll() != BacnetServerPollResult::WritePropertyAckSent ||
      server.poll() != BacnetServerPollResult::CovNotificationSent) {
    return false;
  }
  BacnetCovNotification notification;
  return BacnetProtocol::parseCovNotification(transport.lastSent,
                                              transport.lastSentLength,
                                              notification) &&
         notification.property == BacnetPropertyId::PresentValue &&
         notification.value.type == BacnetValueType::Enumerated &&
         notification.value.unsignedValue == 1;
}

bool testBinaryValueCovFollowsEffectiveValue() {
  TestTransport transport;
  BacnetServer server(transport);
  BacnetServerBinaryValue values[] = {{320, "BV320"}};
  values[0].priority.relinquishDefault = false;
  BacnetServerDevice device;
  device.deviceInstance = 108;
  if (!server.setBinaryValues(values, 1) || !server.begin(device)) {
    return false;
  }
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::BinaryValue), 320};
  const BacnetIpEndpoint peer(192, 0, 2, 31, 47808);
  uint8_t subscribe[BacnetProtocol::kMaxSubscribeCovRequestSize] = {};
  const size_t subscribeSize = BacnetProtocol::buildSubscribeCovRequest(
    subscribe, sizeof(subscribe), 31, object, 30);
  subscribe[8] = 33;
  transport.queue(subscribe, subscribeSize, peer);
  if (server.poll() != BacnetServerPollResult::SubscribeCovAckSent ||
      server.poll() != BacnetServerPollResult::CovNotificationSent) {
    return false;
  }

  BacnetValue inactive;
  inactive.type = BacnetValueType::Enumerated;
  inactive.unsignedValue = 0;
  BacnetValue active;
  active.type = BacnetValueType::Enumerated;
  active.unsignedValue = 1;
  BacnetValue nullValue;
  nullValue.type = BacnetValueType::Null;
  BacnetWritePropertyOptions priority8;
  priority8.hasPriority = true;
  priority8.priority = 8;
  BacnetWritePropertyOptions priority16;
  priority16.hasPriority = true;
  priority16.priority = 16;
  uint8_t write[BacnetProtocol::kMaxWritePropertyRequestSize] = {};
  size_t writeSize = BacnetProtocol::buildWritePropertyRequest(
    write, sizeof(write), object, BacnetPropertyId::PresentValue, active, priority8, 34);
  transport.queue(write, writeSize, peer);
  if (server.poll() != BacnetServerPollResult::WritePropertyAckSent ||
      server.poll() != BacnetServerPollResult::CovNotificationSent) {
    return false;
  }
  BacnetCovNotification notification;
  if (!BacnetProtocol::parseCovNotification(transport.lastSent,
                                            transport.lastSentLength,
                                            notification) ||
      notification.property != BacnetPropertyId::PresentValue ||
      notification.value.type != BacnetValueType::Enumerated ||
      notification.value.unsignedValue != 1) {
    return false;
  }

  writeSize = BacnetProtocol::buildWritePropertyRequest(
    write, sizeof(write), object, BacnetPropertyId::PresentValue, inactive, priority16, 35);
  transport.queue(write, writeSize, peer);
  if (server.poll() != BacnetServerPollResult::WritePropertyAckSent ||
      server.poll() != BacnetServerPollResult::NoDatagram) {
    return false;
  }

  writeSize = BacnetProtocol::buildWritePropertyRequest(
    write, sizeof(write), object, BacnetPropertyId::PresentValue, nullValue, priority8, 36);
  transport.queue(write, writeSize, peer);
  if (server.poll() != BacnetServerPollResult::WritePropertyAckSent ||
      server.poll() != BacnetServerPollResult::CovNotificationSent ||
      !BacnetProtocol::parseCovNotification(transport.lastSent,
                                            transport.lastSentLength,
                                            notification)) {
    return false;
  }
  return notification.property == BacnetPropertyId::PresentValue &&
         notification.value.type == BacnetValueType::Enumerated &&
         notification.value.unsignedValue == 0;
}

bool testCovNotificationSendFailureUsesBackoffAndRetainsChange() {
  TestTransport transport;
  TestClock clock;
  BacnetServer server(transport);
  server.setClock(&clock);
  BacnetServerAnalogInput inputs[] = {{0, "Input", 1.0F, BacnetEngineeringUnits::Percent}};
  BacnetServerDevice device;
  device.deviceInstance = 106;
  device.apduTimeout = 100;
  device.numberOfApduRetries = 2;
  if (!server.setAnalogInputs(inputs, 1) || !server.begin(device)) {
    return false;
  }
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::AnalogInput), 0};
  const BacnetIpEndpoint peer(192, 0, 2, 12, 47812);
  uint8_t request[BacnetProtocol::kMaxSubscribeCovRequestSize] = {};
  const size_t requestSize = BacnetProtocol::buildSubscribeCovRequest(
    request, sizeof(request), 16, object, 30);
  if (requestSize == 0) {
    return false;
  }
  request[8] = 18;
  transport.queue(request, requestSize, peer);
  if (server.poll() != BacnetServerPollResult::SubscribeCovAckSent) {
    return false;
  }

  transport.sendResult = false;
  const uint32_t sendsBeforeFailure = transport.sendCalls;
  if (server.poll() != BacnetServerPollResult::SendFailed ||
      transport.sendCalls != sendsBeforeFailure + 1U) {
    return false;
  }
  clock.now = 99;
  if (server.poll() != BacnetServerPollResult::NoDatagram ||
      transport.sendCalls != sendsBeforeFailure + 1U) {
    return false;
  }

  transport.sendResult = true;
  clock.now = 100;
  if (server.poll() != BacnetServerPollResult::CovNotificationSent ||
      transport.sendCalls != sendsBeforeFailure + 2U) {
    return false;
  }
  BacnetCovNotification notification;
  if (!BacnetProtocol::parseCovNotification(
        transport.lastSent, transport.lastSentLength, notification) ||
      notification.value.type != BacnetValueType::Real ||
      notification.value.realValue != 1.0F) {
    return false;
  }
  return server.poll() == BacnetServerPollResult::NoDatagram;
}

bool testCovNotificationSendFailureCapsExtremeBackoff() {
  TestTransport transport;
  TestClock clock;
  BacnetServer server(transport);
  server.setClock(&clock);
  BacnetServerAnalogInput inputs[] = {{0, "Input", 1.0F, BacnetEngineeringUnits::Percent}};
  BacnetServerDevice device;
  device.deviceInstance = 107;
  device.apduTimeout = std::numeric_limits<uint32_t>::max();
  device.numberOfApduRetries = 2;
  if (!server.setAnalogInputs(inputs, 1) || !server.begin(device)) {
    return false;
  }
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::AnalogInput), 0};
  const BacnetIpEndpoint peer(192, 0, 2, 13, 47813);
  uint8_t request[BacnetProtocol::kMaxSubscribeCovRequestSize] = {};
  const size_t requestSize = BacnetProtocol::buildSubscribeCovRequest(
    request, sizeof(request), 17, object, 30);
  if (requestSize == 0) {
    return false;
  }
  request[8] = 19;
  transport.queue(request, requestSize, peer);
  if (server.poll() != BacnetServerPollResult::SubscribeCovAckSent) {
    return false;
  }

  transport.sendResult = false;
  const uint32_t sendsBeforeFailure = transport.sendCalls;
  if (server.poll() != BacnetServerPollResult::SendFailed ||
      transport.sendCalls != sendsBeforeFailure + 1U) {
    return false;
  }
  clock.now = 1;
  return server.poll() == BacnetServerPollResult::NoDatagram &&
         transport.sendCalls == sendsBeforeFailure + 1U;
}

} // namespace

int main() {
  return testSubscribeCovLifecycle() && testSubscribeCovPropertyAndConfirmedAck() &&
             testSubscribeCovPropertyIncrement() &&
             testBinaryOutputCovTracksEffectivePresentValue() &&
             testSubscriptionCapacityAndErrors() && testBinaryOutputCovFollowsEffectiveValue() &&
             testBinaryValueCovFollowsEffectiveValue() &&
             testCovNotificationSendFailureUsesBackoffAndRetainsChange() &&
             testCovNotificationSendFailureCapsExtremeBackoff()
           ? 0
           : 1;
}
