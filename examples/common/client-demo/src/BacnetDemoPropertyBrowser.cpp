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

void BacnetDemoPropertyBrowser::reset(BacnetDeviceSession* session) {
  if (session != nullptr) {
    cancelLoad(*session);
  }
  subscription_.reset();
  object_ = BacnetObjectId{};
  summary_ = BacnetPropertyReadAllResult{};
  for (size_t i = 0; i < kMaxAdvertisedProperties; ++i) {
    advertisedProperties_[i] = BacnetPropertyId::ObjectName;
  }
  for (size_t i = 0; i < kMaxProperties; ++i) {
    selectedProperties_[i] = BacnetPropertyId::ObjectName;
    rows_[i] = BacnetPropertyReadResult{};
  }
  timeoutMs_ = 0;
  advertisedIndex_ = 0;
  selectedPropertyCount_ = 0;
  propertyReadIndex_ = 0;
  readResultHandled_ = false;
  rowCount_ = 0;
  selectedIndex_ = kMaxProperties;
  usingFallbackProperties_ = false;
  loadState_ = LoadState::Idle;
}

bool BacnetDemoPropertyBrowser::queueLoad(BacnetObjectId object,
                                          uint32_t timeoutMs) {
  if (loading()) {
    return false;
  }
  subscription_.reset();
  object_ = object;
  summary_ = BacnetPropertyReadAllResult{};
  rowCount_ = 0;
  selectedIndex_ = kMaxProperties;
  selectedPropertyCount_ = 0;
  propertyReadIndex_ = 0;
  readResultHandled_ = false;
  advertisedIndex_ = 0;
  timeoutMs_ = timeoutMs;
  usingFallbackProperties_ = false;
  loadState_ = LoadState::Queued;
  return true;
}

void BacnetDemoPropertyBrowser::cancelLoad(BacnetDeviceSession& session) {
  if (readJob_.isActive()) {
    session.cancelPropertyRead(readJob_);
  }
  if (loading()) {
    loadState_ = LoadState::Cancelled;
  }
}

void BacnetDemoPropertyBrowser::beginQueuedLoad(BacnetDeviceSession& session,
                                                uint32_t nowMs) {
  if (session.isBusy()) {
    return;
  }
  loadState_ = LoadState::ReadingPropertyList;
  const BacnetPropertyRequest request{
    object_, BacnetPropertyId::PropertyList, 0};
  if (!session.beginPropertyRead(readJob_, request, timeoutMs_, nowMs)) {
    failLoad(BacnetPropertyReadStatus::SendFailed);
  } else {
    readResultHandled_ = false;
  }
}

void BacnetDemoPropertyBrowser::startNextPropertyListRead(
  BacnetDeviceSession& session,
  uint32_t nowMs) {
  if (advertisedIndex_ >= summary_.advertised) {
    selectPropertiesFromAdvertised();
    if (selectedPropertyCount_ == 0) {
      completeLoad();
      return;
    }
    loadState_ = LoadState::ReadingProperties;
    return;
  }
  const BacnetPropertyRequest request{
    object_, BacnetPropertyId::PropertyList, static_cast<uint32_t>(advertisedIndex_ + 1)};
  if (!session.beginPropertyRead(readJob_, request, timeoutMs_, nowMs)) {
    failLoad(BacnetPropertyReadStatus::SendFailed);
  } else {
    readResultHandled_ = false;
  }
}

void BacnetDemoPropertyBrowser::startNextPropertyRead(BacnetDeviceSession& session,
                                                      uint32_t nowMs) {
  if (propertyReadIndex_ >= selectedPropertyCount_) {
    completeLoad();
    return;
  }
  const BacnetPropertyRequest request{
    object_, selectedProperties_[propertyReadIndex_], kBacnetNoArrayIndex};
  if (!session.beginPropertyRead(readJob_, request, timeoutMs_, nowMs)) {
    rows_[propertyReadIndex_].propertyId = selectedProperties_[propertyReadIndex_];
    rows_[propertyReadIndex_].status = BacnetPropertyReadStatus::SendFailed;
    ++propertyReadIndex_;
    ++summary_.attempted;
    ++summary_.stored;
    ++summary_.failed;
  } else {
    readResultHandled_ = false;
  }
}

void BacnetDemoPropertyBrowser::processPropertyListRead() {
  const BacnetPropertyReadStatus status = readJob_.readStatus();
  if (readJob_.request().arrayIndex == 0) {
    summary_.propertyListStatus = status;
    if (status == BacnetPropertyReadStatus::UnsupportedProperty) {
      selectFallbackProperties();
      if (selectedPropertyCount_ == 0) {
        completeLoad();
      } else {
        loadState_ = LoadState::ReadingProperties;
      }
      return;
    }
    if (status != BacnetPropertyReadStatus::Ack ||
        readJob_.value().type != BacnetValueType::Unsigned) {
      failLoad(status == BacnetPropertyReadStatus::Ack
                 ? BacnetPropertyReadStatus::DecodeError
                 : status);
      return;
    }
    summary_.advertised = readJob_.value().unsignedValue;
    const size_t boundedCount = summary_.advertised < kMaxAdvertisedProperties
                                  ? static_cast<size_t>(summary_.advertised)
                                  : kMaxAdvertisedProperties;
    summary_.truncated = summary_.advertised > boundedCount;
    advertisedIndex_ = 0;
    if (boundedCount == 0) {
      completeLoad();
    }
    return;
  }

  if (status != BacnetPropertyReadStatus::Ack) {
    failLoad(status);
    return;
  }
  BacnetPropertyId property = BacnetPropertyId::ObjectName;
  if (!bacnetPropertyIdFromPropertyListValue(readJob_.value(), property)) {
    failLoad(BacnetPropertyReadStatus::DecodeError);
    return;
  }
  if (advertisedIndex_ < kMaxAdvertisedProperties) {
    advertisedProperties_[advertisedIndex_] = property;
    ++summary_.collected;
  }
  ++advertisedIndex_;
  const size_t boundedCount = summary_.advertised < kMaxAdvertisedProperties
                                ? static_cast<size_t>(summary_.advertised)
                                : kMaxAdvertisedProperties;
  if (advertisedIndex_ >= boundedCount) {
    selectPropertiesFromAdvertised();
    if (selectedPropertyCount_ == 0) {
      completeLoad();
    } else {
      loadState_ = LoadState::ReadingProperties;
    }
  }
}

void BacnetDemoPropertyBrowser::processPropertyRead() {
  if (propertyReadIndex_ >= kMaxProperties) {
    completeLoad();
    return;
  }
  BacnetPropertyReadResult& resultRow = rows_[propertyReadIndex_];
  resultRow = BacnetPropertyReadResult{};
  resultRow.propertyId = selectedProperties_[propertyReadIndex_];
  resultRow.status = readJob_.readStatus();
  resultRow.value = readJob_.value();
  resultRow.errorClass = readJob_.errorClass();
  resultRow.errorCode = readJob_.errorCode();
  ++summary_.attempted;
  ++summary_.stored;
  if (resultRow.status == BacnetPropertyReadStatus::Ack) {
    ++summary_.acked;
  } else {
    ++summary_.failed;
  }
  ++propertyReadIndex_;
  rowCount_ = propertyReadIndex_;
  if (propertyReadIndex_ >= selectedPropertyCount_) {
    completeLoad();
  }
}

void BacnetDemoPropertyBrowser::selectPropertiesFromAdvertised() {
  selectedPropertyCount_ = selectAdvertisedProperties(
    object_, advertisedProperties_, summary_.collected, selectedProperties_, kMaxProperties);
  summary_.truncated = summary_.truncated || summary_.collected > selectedPropertyCount_;
  propertyReadIndex_ = 0;
}

void BacnetDemoPropertyBrowser::selectFallbackProperties() {
  selectedPropertyCount_ = preferredPropertiesForObject(
    object_, selectedProperties_, kMaxProperties);
  propertyReadIndex_ = 0;
  usingFallbackProperties_ = selectedPropertyCount_ > 0;
}

void BacnetDemoPropertyBrowser::failLoad(BacnetPropertyReadStatus status) {
  summary_.propertyListStatus = status;
  loadState_ = LoadState::Failed;
}

void BacnetDemoPropertyBrowser::completeLoad() {
  rowCount_ = propertyReadIndex_ < kMaxProperties ? propertyReadIndex_ : kMaxProperties;
  loadState_ = LoadState::Complete;
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
  selectedPropertyCount_ = rowCount_;
  propertyReadIndex_ = rowCount_;
  selectedIndex_ = kMaxProperties;
  usingFallbackProperties_ = false;
  loadState_ = LoadState::Complete;

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
  if (loadState_ == LoadState::Queued) {
    // Complete a previously scheduled subscription transaction without
    // starting another background request before the browser claims the slot.
    session.pollInFlight(nowMs);
    beginQueuedLoad(session, nowMs);
    return;
  }

  if (loadState_ == LoadState::ReadingPropertyList) {
    if (readJob_.isActive()) {
      session.pollPropertyRead(readJob_, nowMs);
      return;
    }
    if (readJob_.isTerminal() && !readResultHandled_) {
      readResultHandled_ = true;
      processPropertyListRead();
      return;
    }
    startNextPropertyListRead(session, nowMs);
    return;
  }

  if (loadState_ == LoadState::ReadingProperties) {
    if (readJob_.isActive()) {
      session.pollPropertyRead(readJob_, nowMs);
      return;
    }
    if (readJob_.isTerminal() && !readResultHandled_) {
      readResultHandled_ = true;
      processPropertyRead();
      return;
    }
    startNextPropertyRead(session, nowMs);
    return;
  }

  if (subscription_ != nullptr) {
    session.poll(*subscription_, nowMs);
  }
}

bool BacnetDemoPropertyBrowser::loaded() const {
  return loadState_ == LoadState::Complete && rowCount_ > 0;
}

bool BacnetDemoPropertyBrowser::loading() const {
  return loadState_ == LoadState::Queued ||
         loadState_ == LoadState::ReadingPropertyList ||
         loadState_ == LoadState::ReadingProperties;
}

BacnetDemoPropertyBrowser::LoadState BacnetDemoPropertyBrowser::loadState() const {
  return loadState_;
}

const char* BacnetDemoPropertyBrowser::loadStateText() const {
  switch (loadState_) {
    case LoadState::Idle:
      return "idle";
    case LoadState::Queued:
      return "queued";
    case LoadState::ReadingPropertyList:
      return "reading-property-list";
    case LoadState::ReadingProperties:
      return "reading-properties";
    case LoadState::Complete:
      return "complete";
    case LoadState::Failed:
      return "failed";
    case LoadState::Cancelled:
      return "cancelled";
  }
  return "unknown";
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
