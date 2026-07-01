// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetDeviceSession.h"

#include "BacnetRemoteObject.h"

BacnetDeviceSession::BacnetDeviceSession(BacnetClient& client,
                                         uint32_t deviceInstance,
                                         IPAddress address, uint16_t port)
    : client_(client),
      deviceInstance_(deviceInstance),
      address_(address),
      port_(port) {}

BacnetClient& BacnetDeviceSession::client() {
  return client_;
}

const BacnetClient& BacnetDeviceSession::client() const {
  return client_;
}

uint32_t BacnetDeviceSession::deviceInstance() const {
  return deviceInstance_;
}

IPAddress BacnetDeviceSession::address() const {
  return address_;
}

uint16_t BacnetDeviceSession::port() const {
  return port_;
}

BacnetObjectId BacnetDeviceSession::deviceObject() const {
  return BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::Device),
                        deviceInstance_};
}

BacnetRemoteObject BacnetDeviceSession::object(BacnetObjectId objectId) {
  return BacnetRemoteObject(*this, objectId);
}

BacnetRemoteObject BacnetDeviceSession::object(BacnetObjectType objectType,
                                               uint32_t objectInstance) {
  return object(
      BacnetObjectId{static_cast<uint16_t>(objectType), objectInstance});
}

BacnetDeviceSessionReadStatus BacnetDeviceSession::readProperty(
    BacnetObjectId object, BacnetPropertyId property, BacnetValue& value,
    uint32_t timeoutMs, uint32_t arrayIndex) {
  value = BacnetValue{};
  const BacnetPropertyRequest request{object, property, arrayIndex};
  const uint8_t invokeId = allocateInvokeId();

  if (!client_.sendReadProperty(address_, request, invokeId, port_)) {
    return BacnetDeviceSessionReadStatus::SendFailed;
  }

  const unsigned long startedAt = millis();
  while (true) {
    const BacnetReadPropertyPollStatus status =
        client_.pollReadPropertyStatus(value, invokeId, request);
    if (status == BacnetReadPropertyPollStatus::Ack) {
      return BacnetDeviceSessionReadStatus::Ack;
    }
    if (status == BacnetReadPropertyPollStatus::Error) {
      return BacnetDeviceSessionReadStatus::Error;
    }
    if (millis() - startedAt >= timeoutMs) {
      client_.logReadPropertyTimeout(invokeId, request);
      return BacnetDeviceSessionReadStatus::Timeout;
    }
    yield();
  }
}

BacnetDeviceSessionReadStatus BacnetDeviceSession::readProperty(
    BacnetObjectType objectType, uint32_t objectInstance,
    BacnetPropertyId property, BacnetValue& value, uint32_t timeoutMs,
    uint32_t arrayIndex) {
  return readProperty(
      BacnetObjectId{static_cast<uint16_t>(objectType), objectInstance},
      property, value, timeoutMs, arrayIndex);
}

uint8_t BacnetDeviceSession::allocateInvokeId() {
  const uint8_t invokeId = nextInvokeId_;
  ++nextInvokeId_;
  if (nextInvokeId_ == 0) {
    nextInvokeId_ = 1;
  }
  return invokeId;
}
