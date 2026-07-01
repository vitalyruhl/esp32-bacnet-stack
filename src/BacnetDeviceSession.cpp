// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetDeviceSession.h"

#include "BacnetRemoteObject.h"

namespace {

bool objectIdFromObjectListValue(const BacnetValue& value,
                                 BacnetObjectId& objectId) {
  if (value.type != BacnetValueType::ObjectIdentifier) {
    return false;
  }
  objectId = value.objectValue;
  return true;
}

}  // namespace

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

BacnetObjectScanResult BacnetDeviceSession::scanObjectList(
    const BacnetObjectScanOptions& options,
    BacnetScannedObject* results,
    size_t resultCapacity) {
  BacnetObjectScanResult result;
  BacnetValue countValue;
  result.objectListCountStatus =
      readProperty(deviceObject(), BacnetPropertyId::ObjectList, countValue,
                   options.readTimeoutMs, 0);
  if (result.objectListCountStatus != BacnetDeviceSessionReadStatus::Ack) {
    return result;
  }
  if (countValue.type != BacnetValueType::Unsigned) {
    result.objectListCountStatus = BacnetDeviceSessionReadStatus::Error;
    return result;
  }

  result.objectListCount = countValue.unsignedValue;
  const uint32_t maxEntries =
      result.objectListCount < options.maxObjectListEntries
          ? result.objectListCount
          : options.maxObjectListEntries;

  for (uint32_t index = 1; index <= maxEntries; ++index) {
    BacnetValue entryValue;
    const BacnetDeviceSessionReadStatus entryStatus =
        readProperty(deviceObject(), BacnetPropertyId::ObjectList, entryValue,
                     options.readTimeoutMs, index);
    if (entryStatus != BacnetDeviceSessionReadStatus::Ack) {
      break;
    }

    ++result.inspected;
    BacnetObjectId objectId;
    if (!objectIdFromObjectListValue(entryValue, objectId) ||
        !options.acceptsObjectType(objectId)) {
      continue;
    }

    ++result.found;
    if (results == nullptr || result.stored >= resultCapacity) {
      result.truncated = true;
      continue;
    }

    BacnetScannedObject& scanned = results[result.stored++];
    scanned = BacnetScannedObject{};
    scanned.objectId = objectId;
    BacnetRemoteObject remoteObject = object(objectId);
    if (options.readObjectName) {
      scanned.objectNameStatus =
          remoteObject.readObjectName(scanned.objectName, options.readTimeoutMs);
    }
    if (options.readDescription) {
      scanned.descriptionStatus =
          remoteObject.readDescription(scanned.description,
                                       options.readTimeoutMs);
    }
    if (options.readPresentValue) {
      scanned.presentValueStatus =
          remoteObject.readPresentValue(scanned.presentValue,
                                        options.readTimeoutMs);
    }
  }

  if (result.objectListCount > options.maxObjectListEntries) {
    result.truncated = true;
  }
  return result;
}

uint8_t BacnetDeviceSession::allocateInvokeId() {
  const uint8_t invokeId = nextInvokeId_;
  ++nextInvokeId_;
  if (nextInvokeId_ == 0) {
    nextInvokeId_ = 1;
  }
  return invokeId;
}

bool BacnetObjectScanOptions::acceptsObjectType(
    BacnetObjectId objectId) const {
  if (objectTypes == nullptr || objectTypeCount == 0) {
    return true;
  }
  for (size_t i = 0; i < objectTypeCount; ++i) {
    if (static_cast<uint16_t>(objectTypes[i]) == objectId.type) {
      return true;
    }
  }
  return false;
}

bool BacnetObjectScanOptions::acceptsObjectType(
    BacnetObjectType objectType) const {
  return acceptsObjectType(
      BacnetObjectId{static_cast<uint16_t>(objectType), 0});
}
