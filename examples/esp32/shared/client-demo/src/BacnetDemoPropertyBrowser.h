// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <BacnetDeviceSession.h>

#include <cstddef>
#include <cstdint>
#include <memory>

class BacnetDemoPropertyBrowser {
public:
  static constexpr size_t kMaxProperties = 8;
  // Keep the discovery input bounded independently from the eight visible
  // rows. This covers the currently observed Device/AV/MSV Property_Lists
  // without moving large BacnetValue instances onto the Arduino loop stack.
  static constexpr size_t kMaxAdvertisedProperties = 64;

  enum class LoadState : uint8_t {
    Idle,
    Queued,
    ReadingPropertyList,
    ReadingProperties,
    Complete,
    Failed,
    Cancelled,
  };

  void reset(BacnetDeviceSession* session = nullptr);
  bool queueLoad(BacnetObjectId object, uint32_t timeoutMs);
  void cancelLoad(BacnetDeviceSession& session);
  void applyReadAllResult(BacnetObjectId object,
                          const BacnetPropertyReadAllResult& summary,
                          const BacnetPropertyReadResult* rows,
                          size_t count);

  bool select(size_t index);
  bool hasSelection() const;
  size_t selectedIndex() const;
  bool subscribe(BacnetDeviceSession& session,
                 const BacnetSubscribeOptions& options);
  void stopSubscription();
  void poll(BacnetDeviceSession& session, uint32_t nowMs);

  bool loaded() const;
  bool loading() const;
  LoadState loadState() const;
  const char* loadStateText() const;
  BacnetObjectId objectId() const;
  const BacnetPropertyReadAllResult& result() const;
  bool usingFallbackProperties() const;
  size_t rowCount() const;
  const BacnetPropertyReadResult* row(size_t index) const;
  const BacnetPropertyReadResult* selectedRow() const;
  const BacnetPropertySubscription* subscription() const;
  const char* subscriptionModeText() const;

private:
  BacnetObjectId object_;
  BacnetPropertyReadAllResult summary_;
  BacnetPropertyId advertisedProperties_[kMaxAdvertisedProperties];
  BacnetPropertyId selectedProperties_[kMaxProperties];
  BacnetPropertyReadResult rows_[kMaxProperties];
  BacnetPropertyReadJob readJob_;
  uint32_t timeoutMs_ = 0;
  size_t advertisedIndex_ = 0;
  size_t selectedPropertyCount_ = 0;
  size_t propertyReadIndex_ = 0;
  bool readResultHandled_ = false;
  size_t rowCount_ = 0;
  size_t selectedIndex_ = kMaxProperties;
  bool usingFallbackProperties_ = false;
  LoadState loadState_ = LoadState::Idle;
  std::unique_ptr<BacnetPropertySubscription> subscription_;

  void beginQueuedLoad(BacnetDeviceSession& session, uint32_t nowMs);
  void processPropertyListRead();
  void processPropertyRead();
  void startNextPropertyListRead(BacnetDeviceSession& session, uint32_t nowMs);
  void startNextPropertyRead(BacnetDeviceSession& session, uint32_t nowMs);
  void selectPropertiesFromAdvertised();
  void selectFallbackProperties();
  void failLoad(BacnetPropertyReadStatus status);
  void completeLoad();
};
