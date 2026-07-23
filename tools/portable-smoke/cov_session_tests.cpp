// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetClient.h"
#include "BacnetDeviceSession.h"
#include "BacnetRemoteObject.h"

#include <cstring>

namespace {

class TestClock final : public BacnetMonotonicClock {
public:
  uint32_t nowMs() const override { return now; }
  uint32_t now = 0;
};

class TestTransport final : public BacnetDatagramTransport {
public:
  bool begin(uint16_t) override { return true; }
  void end() override {}

  bool send(const BacnetIpEndpoint& destination,
            const uint8_t* data,
            size_t length) override {
    ++sendCalls;
    lastDestination = destination;
    if (data == nullptr || length > sizeof(lastSent)) {
      return false;
    }
    std::memcpy(lastSent, data, length);
    lastSentLength = length;
    return sendResult;
  }

  size_t receive(uint8_t* data,
                 size_t capacity,
                 BacnetIpEndpoint& source) override {
    if (incomingLength == 0 || incomingLength > capacity) {
      return 0;
    }
    std::memcpy(data, incoming, incomingLength);
    source = incomingSource;
    const size_t length = incomingLength;
    incomingLength = 0;
    return length;
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

  bool sendResult = true;
  uint32_t sendCalls = 0;
  uint8_t lastSent[256] = {};
  size_t lastSentLength = 0;
  BacnetIpEndpoint lastDestination;

private:
  uint8_t incoming[256] = {};
  size_t incomingLength = 0;
  BacnetIpEndpoint incomingSource;
};

bool expect(bool condition) {
  return condition;
}

bool queueSubscribeAck(TestTransport& transport,
                       uint8_t invokeId,
                       uint8_t service,
                       const BacnetIpEndpoint& source) {
  uint8_t ack[16] = {};
  const size_t length = BacnetProtocol::buildSimpleAckResponse(
    ack, sizeof(ack), invokeId, service);
  if (length == 0) {
    return false;
  }
  transport.queue(ack, length, source);
  return true;
}

bool queueConfirmedNotification(TestTransport& transport,
                                uint32_t processId,
                                BacnetObjectId object,
                                const BacnetIpEndpoint& source,
                                uint8_t invokeId) {
  BacnetCovPropertyValue value;
  value.property = BacnetPropertyId::PresentValue;
  value.value.type = BacnetValueType::Real;
  value.value.realValue = 2.0F;
  uint8_t notification[256] = {};
  const size_t length = BacnetProtocol::buildCovNotification(
    notification,
    sizeof(notification),
    processId,
    BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::Device), 1234},
    object,
    30,
    &value,
    1,
    true,
    invokeId);
  if (length == 0) {
    return false;
  }
  transport.queue(notification, length, source);
  return true;
}

bool testSendFailureBackoffAndRecovery() {
  TestClock clock;
  TestTransport transport;
  BacnetClient client(transport, &clock);
  const BacnetIpEndpoint peer(192, 0, 2, 1, 47808);
  if (!client.begin(47809)) {
    return false;
  }
  BacnetDeviceSession session(client, 1234, peer);
  BacnetSubscribeOptions options;
  options.preferCov = true;
  options.usePropertyCov = true;
  options.immediateFirstRead = false;
  options.fallbackPollMs = 10000;
  options.covRenewBeforeSeconds = 5;
  BacnetPropertySubscription subscription =
    session.object(BacnetObjectType::AnalogInput, 0)
      .property(BacnetPropertyId::PresentValue)
      .subscribe(nullptr, nullptr, options);

  transport.sendResult = false;
  clock.now = 1000;
  session.poll(subscription, clock.now);
  const uint32_t sendsAfterFailure = transport.sendCalls;
  if (!expect(subscription.covStatus() == BacnetCovSubscriptionStatus::SendFailed) ||
      !expect(sendsAfterFailure != 0) ||
      !expect(session.covSubscribeAttemptCount() == 1U) ||
      !expect(session.covSendFailureCount() == 1U)) {
    return false;
  }

  clock.now = 5999;
  session.poll(subscription, clock.now);
  if (!expect(transport.sendCalls == sendsAfterFailure)) {
    return false;
  }

  transport.sendResult = true;
  clock.now = 6000;
  session.poll(subscription, clock.now);
  if (!expect(transport.sendCalls == sendsAfterFailure + 1U) ||
      !expect(subscription.inFlight()) ||
      !expect(session.covSubscribeAttemptCount() == 2U)) {
    return false;
  }
  const uint8_t invokeId = transport.lastSent[8];
  const BacnetIpEndpoint foreignPeer(192, 0, 2, 99, 47808);
  if (!queueSubscribeAck(transport, invokeId, 0x1CU, foreignPeer)) {
    return false;
  }
  session.poll(subscription, 6001);
  if (!expect(subscription.inFlight())) {
    return false;
  }
  if (!queueSubscribeAck(transport, invokeId, 0x1CU, peer)) {
    return false;
  }
  session.poll(subscription, 6002);
  return expect(subscription.covStatus() == BacnetCovSubscriptionStatus::Active);
}

bool testSessionValidatesPeerAndProcessBeforeConfirmedAck() {
  TestClock clock;
  TestTransport transport;
  BacnetClient client(transport, &clock);
  const BacnetIpEndpoint peer(192, 0, 2, 1, 47808);
  const BacnetIpEndpoint foreignPeer(192, 0, 2, 99, 47808);
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::AnalogInput), 0};
  if (!client.begin(47809)) {
    return false;
  }
  BacnetDeviceSession session(client, 1234, peer);
  BacnetSubscribeOptions options;
  options.preferCov = true;
  options.usePropertyCov = true;
  options.immediateFirstRead = false;
  options.issueConfirmedNotifications = true;
  BacnetPropertySubscription subscription =
    session.object(object).property(BacnetPropertyId::PresentValue).subscribe(nullptr, nullptr, options);

  session.poll(subscription, 1000);
  if (!subscription.inFlight() ||
      !queueSubscribeAck(transport, transport.lastSent[8], 0x1CU, peer)) {
    return false;
  }
  session.poll(subscription, 1001);
  if (!expect(subscription.covStatus() == BacnetCovSubscriptionStatus::Active)) {
    return false;
  }

  const uint32_t processId = (static_cast<uint32_t>(object.type) << 22U) | object.instance;
  const uint32_t sendsBeforeForeign = transport.sendCalls;
  const BacnetIpEndpoint wrongPortPeer(192, 0, 2, 1, 47809);
  if (!queueConfirmedNotification(transport, processId, object, wrongPortPeer, 40)) {
    return false;
  }
  session.poll(subscription, 1002);
  if (!expect(transport.sendCalls == sendsBeforeForeign)) {
    return false;
  }

  if (!queueConfirmedNotification(transport, processId, object, foreignPeer, 41)) {
    return false;
  }
  session.poll(subscription, 1003);
  if (!expect(transport.sendCalls == sendsBeforeForeign)) {
    return false;
  }

  if (!queueConfirmedNotification(transport, processId + 1U, object, peer, 42)) {
    return false;
  }
  session.poll(subscription, 1004);
  if (!expect(transport.sendCalls == sendsBeforeForeign)) {
    return false;
  }

  if (!queueConfirmedNotification(transport, processId, object, peer, 43)) {
    return false;
  }
  session.poll(subscription, 1005);
  return expect(transport.sendCalls == sendsBeforeForeign + 1U) &&
         expect(BacnetProtocol::isSimpleAck(
           transport.lastSent, transport.lastSentLength, 43, 0x01U)) &&
         expect(subscription.hasValue());
}

} // namespace

int main() {
  return testSendFailureBackoffAndRecovery() &&
           testSessionValidatesPeerAndProcessBeforeConfirmedAck()
           ? 0
           : 1;
}
