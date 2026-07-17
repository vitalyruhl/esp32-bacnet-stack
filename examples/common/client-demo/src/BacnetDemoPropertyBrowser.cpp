// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetDemoPropertyBrowser.h"

#include <BacnetRemoteObject.h>

namespace {

size_t preferredPropertiesForObject(BacnetObjectId object,
                                    BacnetPropertyId* properties,
                                    size_t capacity) {
  static constexpr BacnetPropertyId kDeviceProperties[] = {
    BacnetPropertyId::ObjectIdentifier,
    BacnetPropertyId::ObjectName,
    BacnetPropertyId::ObjectType,
    BacnetPropertyId::VendorIdentifier,
    BacnetPropertyId::VendorName,
    BacnetPropertyId::ModelName,
    BacnetPropertyId::FirmwareRevision,
    BacnetPropertyId::ProtocolRevision,
  };
  static constexpr BacnetPropertyId kAnalogValueProperties[] = {
    BacnetPropertyId::ObjectName,
    BacnetPropertyId::Description,
    BacnetPropertyId::PresentValue,
    BacnetPropertyId::Units,
    BacnetPropertyId::MinPresentValue,
    BacnetPropertyId::MaxPresentValue,
    BacnetPropertyId::Resolution,
    BacnetPropertyId::CovIncrement,
  };
  static constexpr BacnetPropertyId kMultiStateValueProperties[] = {
    BacnetPropertyId::ObjectName,
    BacnetPropertyId::Description,
    BacnetPropertyId::PresentValue,
    BacnetPropertyId::NumberOfStates,
  };

  const BacnetPropertyId* source = nullptr;
  size_t sourceCount = 0;
  switch (static_cast<BacnetObjectType>(object.type)) {
    case BacnetObjectType::Device:
      source = kDeviceProperties;
      sourceCount = sizeof(kDeviceProperties) / sizeof(kDeviceProperties[0]);
      break;
    case BacnetObjectType::AnalogValue:
      source = kAnalogValueProperties;
      sourceCount = sizeof(kAnalogValueProperties) / sizeof(kAnalogValueProperties[0]);
      break;
    case BacnetObjectType::MultiStateValue:
      source = kMultiStateValueProperties;
      sourceCount = sizeof(kMultiStateValueProperties) / sizeof(kMultiStateValueProperties[0]);
      break;
    default:
      return 0;
  }

  const size_t count = sourceCount < capacity ? sourceCount : capacity;
  for (size_t i = 0; i < count; ++i) {
    properties[i] = source[i];
  }
  return count;
}

bool containsProperty(const BacnetPropertyId* properties,
                      size_t count,
                      BacnetPropertyId property) {
  for (size_t i = 0; i < count; ++i) {
    if (properties[i] == property) {
      return true;
    }
  }
  return false;
}

size_t selectAdvertisedProperties(BacnetObjectId object,
                                  const BacnetPropertyId* advertised,
                                  size_t advertisedCount,
                                  BacnetPropertyId* selected,
                                  size_t selectedCapacity) {
  if (advertised == nullptr || selected == nullptr || selectedCapacity == 0) {
    return 0;
  }

  BacnetPropertyId preferred[BacnetDemoPropertyBrowser::kMaxProperties] = {};
  const size_t preferredCount = preferredPropertiesForObject(
    object, preferred, BacnetDemoPropertyBrowser::kMaxProperties);
  size_t selectedCount = 0;

  // The UI remains bounded, but its rows are selected exclusively from the
  // object-advertised Property_List. Common properties are shown first so a
  // user can select Present_Value for a COV subscription when it is offered.
  for (size_t i = 0; i < preferredCount && selectedCount < selectedCapacity; ++i) {
    if (containsProperty(advertised, advertisedCount, preferred[i])) {
      selected[selectedCount++] = preferred[i];
    }
  }
  for (size_t i = 0; i < advertisedCount && selectedCount < selectedCapacity; ++i) {
    if (!containsProperty(selected, selectedCount, advertised[i])) {
      selected[selectedCount++] = advertised[i];
    }
  }
  return selectedCount;
}

} // namespace

void BacnetDemoPropertyBrowser::reset() {
  subscription_.reset();
  object_ = BacnetObjectId{};
  summary_ = BacnetPropertyReadAllResult{};
  for (size_t i = 0; i < kMaxAdvertisedProperties; ++i) {
    advertisedProperties_[i] = BacnetPropertyId::ObjectName;
  }
  for (size_t i = 0; i < kMaxProperties; ++i) {
    rows_[i] = BacnetPropertyReadResult{};
  }
  rowCount_ = 0;
  selectedIndex_ = kMaxProperties;
  usingFallbackProperties_ = false;
}

void BacnetDemoPropertyBrowser::load(BacnetDeviceSession& session,
                                     BacnetObjectId object,
                                     uint32_t timeoutMs) {
  BacnetPropertyId properties[kMaxProperties] = {};
  // BacnetValue retains a bounded display buffer. Keep the eight result rows
  // in this browser's persistent storage instead of placing several kilobytes
  // on the Arduino loop stack during a web-triggered browser load.
  BacnetPropertyReadResult* const rows = rows_;

  const BacnetPropertyListReadResult propertyList = session.object(object).readPropertyList(
    advertisedProperties_, kMaxAdvertisedProperties, timeoutMs);
  BacnetPropertyReadAllResult advertisedResult;
  advertisedResult.propertyListStatus = propertyList.status;
  advertisedResult.advertised = propertyList.advertised;
  advertisedResult.collected = propertyList.stored;
  advertisedResult.truncated = propertyList.truncated;

  if (propertyList.status == BacnetPropertyReadStatus::Ack) {
    const size_t propertyCount = selectAdvertisedProperties(
      object, advertisedProperties_, propertyList.stored, properties, kMaxProperties);
    BacnetPropertyReadAllResult operationResult = session.readAllProperties(
      object, properties, propertyCount, rows, kMaxProperties, timeoutMs);
    operationResult.propertyListStatus = propertyList.status;
    operationResult.advertised = propertyList.advertised;
    operationResult.collected = propertyList.stored;
    operationResult.truncated = operationResult.truncated || propertyList.truncated ||
                                propertyList.stored > propertyCount;
    applyReadAllResult(object, operationResult, rows, operationResult.stored);
    return;
  }

  // A bounded profile is only a compatibility path for an object that
  // explicitly reports that Property_List is unsupported. It must never hide
  // an addressing, timeout, or decode failure of the requested object.
  if (advertisedResult.propertyListStatus !=
      BacnetPropertyReadStatus::UnsupportedProperty) {
    applyReadAllResult(object, advertisedResult, rows, advertisedResult.stored);
    return;
  }

  const size_t propertyCount = preferredPropertiesForObject(
    object, properties, kMaxProperties);
  if (propertyCount > 0) {
    const BacnetPropertyReadAllResult operationResult = session.readAllProperties(
      object, properties, propertyCount, rows, kMaxProperties, timeoutMs);
    applyReadAllResult(object, operationResult, rows, operationResult.stored);
    usingFallbackProperties_ = true;
    return;
  }

  applyReadAllResult(
    object, advertisedResult, rows, advertisedResult.stored);
}

void BacnetDemoPropertyBrowser::applyReadAllResult(
  BacnetObjectId object,
  const BacnetPropertyReadAllResult& operationResult,
  const BacnetPropertyReadResult* rows,
  size_t count) {
  subscription_.reset();
  object_ = object;
  summary_ = operationResult;
  rowCount_ = count < kMaxProperties ? count : kMaxProperties;
  selectedIndex_ = kMaxProperties;
  usingFallbackProperties_ = false;

  for (size_t i = 0; i < rowCount_; ++i) {
    rows_[i] = rows != nullptr ? rows[i] : BacnetPropertyReadResult{};
  }
  for (size_t i = rowCount_; i < kMaxProperties; ++i) {
    rows_[i] = BacnetPropertyReadResult{};
  }
}

bool BacnetDemoPropertyBrowser::select(size_t index) {
  if (index >= rowCount_) {
    selectedIndex_ = kMaxProperties;
    return false;
  }
  selectedIndex_ = index;
  return true;
}

bool BacnetDemoPropertyBrowser::hasSelection() const {
  return selectedIndex_ < rowCount_;
}

size_t BacnetDemoPropertyBrowser::selectedIndex() const {
  return hasSelection() ? selectedIndex_ : kMaxProperties;
}

bool BacnetDemoPropertyBrowser::subscribe(
  BacnetDeviceSession& session,
  const BacnetSubscribeOptions& options) {
  if (!hasSelection()) {
    return false;
  }

  subscription_.reset(new BacnetPropertySubscription(
    session.object(object_).property(rows_[selectedIndex_].propertyId).subscribe(nullptr, nullptr, options)));
  return subscription_ != nullptr && subscription_->active();
}

void BacnetDemoPropertyBrowser::stopSubscription() {
  subscription_.reset();
}

void BacnetDemoPropertyBrowser::poll(BacnetDeviceSession& session,
                                     uint32_t nowMs) {
  if (subscription_ != nullptr) {
    session.poll(*subscription_, nowMs);
  }
}

bool BacnetDemoPropertyBrowser::loaded() const {
  return rowCount_ > 0;
}

BacnetObjectId BacnetDemoPropertyBrowser::objectId() const {
  return object_;
}

const BacnetPropertyReadAllResult& BacnetDemoPropertyBrowser::result() const {
  return summary_;
}

bool BacnetDemoPropertyBrowser::usingFallbackProperties() const {
  return usingFallbackProperties_;
}

size_t BacnetDemoPropertyBrowser::rowCount() const {
  return rowCount_;
}

const BacnetPropertyReadResult* BacnetDemoPropertyBrowser::row(size_t index) const {
  return index < rowCount_ ? &rows_[index] : nullptr;
}

const BacnetPropertyReadResult* BacnetDemoPropertyBrowser::selectedRow() const {
  return hasSelection() ? &rows_[selectedIndex_] : nullptr;
}

const BacnetPropertySubscription* BacnetDemoPropertyBrowser::subscription() const {
  return subscription_.get();
}

const char* BacnetDemoPropertyBrowser::subscriptionModeText() const {
  if (subscription_ == nullptr) {
    return "disabled";
  }
  if (!subscription_->active()) {
    return "failed";
  }
  switch (subscription_->covStatus()) {
    case BacnetCovSubscriptionStatus::Active:
      return "SubscribeCOV";
    case BacnetCovSubscriptionStatus::Pending:
      return "SubscribeCOV pending";
    default:
      break;
  }
  if (subscription_->covStatus() == BacnetCovSubscriptionStatus::Error) {
    return "Polling fallback (error)";
  }
  if (subscription_->covStatus() == BacnetCovSubscriptionStatus::Reject) {
    return "Polling fallback (reject)";
  }
  if (subscription_->covStatus() == BacnetCovSubscriptionStatus::Abort) {
    return "Polling fallback (abort)";
  }
  if (subscription_->covStatus() == BacnetCovSubscriptionStatus::Timeout) {
    return "Polling fallback (timeout)";
  }
  if (subscription_->covStatus() == BacnetCovSubscriptionStatus::SendFailed) {
    return "Polling fallback (send-failed)";
  }
  return "failed";
}
