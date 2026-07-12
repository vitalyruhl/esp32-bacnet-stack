// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <Arduino.h>
#include <BacnetDisplayText.h>
#include <BacnetRemoteObject.h>

#include <memory>

enum class BacnetDemoLogLevel : uint8_t {
  Info,
  Warn,
};

using BacnetDemoLogCallback = void (*)(BacnetDemoLogLevel level,
                                       const char* message);

class BacnetDemoWatchedAnalogValue {
public:
  BacnetDemoWatchedAnalogValue(uint32_t objectInstance,
                               uint32_t pollMs,
                               uint32_t timeoutMs);

  void setLogger(BacnetDemoLogCallback logger);
  void reset(const char* status = "not read");
  bool setup(BacnetDeviceSession& session);
  void poll(BacnetDeviceSession& session, uint32_t now);

  String objectSummary() const;
  String labelSummary() const;
  String descriptionSummary() const;
  String valueSummary() const;
  String engineeringUnitSummary() const;
  String minMaxSummary() const;
  String resolutionSummary() const;
  String covIncrementSummary() const;
  String metadataSummary() const;
  String readStatusSummary() const;
  String alarmStateSummary() const;
  String statusSummary() const;
  String refreshSummary() const;
  String lastAttemptAgeSummary() const;
  String lastSuccessAgeSummary() const;

private:
  struct Preview {
    struct MetadataField {
      BacnetPropertyReadStatus status = BacnetPropertyReadStatus::Skipped;
      BacnetValue value;
      bool hasValue = false;
    };

    bool configured = false;
    BacnetObjectId object;
    BacnetValue objectName;
    BacnetDeviceSessionReadStatus objectNameStatus =
      BacnetDeviceSessionReadStatus::Skipped;
    BacnetValue description;
    BacnetDeviceSessionReadStatus descriptionStatus =
      BacnetDeviceSessionReadStatus::Skipped;
    BacnetValue presentValue;
    BacnetPropertyReadStatus presentValueStatus =
      BacnetPropertyReadStatus::Skipped;
    bool hasPresentValue = false;
    String lastStatus = "skipped";
    String alarmState = "unknown";
    String state = "Unknown";
    String flags = "skipped";
    String eventState = "skipped";
    String reliability = "skipped";
    String outOfService = "skipped";
    bool engineeringUnitKnown = false;
    uint32_t engineeringUnitId = 0;
    const char* engineeringUnitSymbol = nullptr;
    BacnetPropertyReadStatus engineeringUnitStatus =
      BacnetPropertyReadStatus::Skipped;
    bool hasEngineeringUnit = false;
    MetadataField minPresentValue;
    MetadataField maxPresentValue;
    MetadataField resolution;
    MetadataField covIncrement;
    uint32_t lastAttemptMs = 0;
    uint32_t lastSuccessMs = 0;
    uint32_t nextStatusRefreshMs = 0;
    size_t nextStatusPropertyIndex = 0;
    bool statusRefreshComplete = false;
    BacnetPropertyId statusRefreshProperty = BacnetPropertyId::StatusFlags;
    std::unique_ptr<BacnetPropertySubscription> presentValueSubscription;
    std::unique_ptr<BacnetPropertySubscription> statusSubscription;
  };

  static void onSubscriptionUpdate(
    const BacnetSubscriptionNotification& notification);

  BacnetProcessObject watchedObject(BacnetDeviceSession& session) const;
  void resetPreview(const char* status);
  void log(BacnetDemoLogLevel level, const String& message) const;
  void handleSubscriptionUpdate(
    const BacnetSubscriptionNotification& notification);
  void appendUnitSuffix(String& text) const;
  static String metadataValueCore(const Preview::MetadataField& field);
  String metadataValueText(const Preview::MetadataField& field) const;
  String cachedValueWithUnit(const BacnetValue& value,
                             BacnetPropertyReadStatus status,
                             bool hasValue) const;
  static String metadataStatusText(BacnetPropertyReadStatus status);
  void updateEngineeringUnit(BacnetPropertyReadStatus status,
                             const BacnetValue& value,
                             bool hasValue);
  static void updateMetadataField(Preview::MetadataField& field,
                                  BacnetProcessObject watched,
                                  BacnetPropertyId propertyId,
                                  BacnetPropertyReadStatus status);
  static String alarmStateFromStatus(const BacnetObjectStatus& status,
                                     bool hasStatusFlags,
                                     bool hasEventState,
                                     bool hasOutOfService);
  void updateFromCache();
  void readIdentity(BacnetProcessObject watched);
  BacnetPropertyReadStatus readMetadataProperty(BacnetProcessObject watched,
                                                BacnetPropertyId propertyId,
                                                BacnetValue& value) const;
  void readMetadata(BacnetProcessObject watched);
  std::unique_ptr<BacnetPropertySubscription> subscribeProperty(
    BacnetProcessObject watched,
    BacnetPropertyId propertyId,
    bool repeating);
  void startStatusRefresh(BacnetDeviceSession& session, uint32_t now);

  uint32_t objectInstance_;
  uint32_t pollMs_;
  uint32_t timeoutMs_;
  BacnetDeviceSession* session_ = nullptr;
  BacnetDemoLogCallback logger_ = nullptr;
  Preview preview_;
};
