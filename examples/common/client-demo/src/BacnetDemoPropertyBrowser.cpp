// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetDemoPropertyBrowser.h"

#include <BacnetRemoteObject.h>

namespace {

size_t fallbackPropertiesForObject(BacnetObjectId object,
                                   BacnetPropertyId* properties,
                                   size_t capacity) {
  static constexpr BacnetPropertyId kDeviceProperties[] = {
    BacnetPropertyId::ObjectName,
    BacnetPropertyId::VendorName,
    BacnetPropertyId::ModelName,
    BacnetPropertyId::FirmwareRevision,
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

}  // namespace

void BacnetDemoPropertyBrowser::reset() {
  subscription_.reset();
  object_ = BacnetObjectId{};
  summary_ = BacnetPropertyReadAllResult{};
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
  BacnetPropertyReadResult rows[kMaxProperties] = {};
  const size_t propertyCount = fallbackPropertiesForObject(
    object, properties, kMaxProperties);
  if (propertyCount > 0) {
    const BacnetPropertyReadAllResult operationResult = session.readAllProperties(
      object, properties, propertyCount, rows, kMaxProperties, timeoutMs);
    applyReadAllResult(object, operationResult, rows, operationResult.stored);
    usingFallbackProperties_ = true;
    return;
  }

  const BacnetPropertyReadAllResult operationResult =
    session.object(object).readAllAdvertisedProperties(
      properties, kMaxProperties, rows, kMaxProperties, timeoutMs);
  applyReadAllResult(object, operationResult, rows, operationResult.stored);
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
    session.object(object_).property(rows_[selectedIndex_].propertyId)
      .subscribe(nullptr, nullptr, options)));
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
