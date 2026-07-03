// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetDemoWatchedAnalogValue.h"

#include "BacnetDemoFormat.h"

namespace {

static const BacnetPropertyId kWatchedAnalogStatusProperties[] = {
  BacnetPropertyId::StatusFlags,
  BacnetPropertyId::EventState,
  BacnetPropertyId::Reliability,
  BacnetPropertyId::OutOfService,
};

} // namespace

BacnetDemoWatchedAnalogValue::BacnetDemoWatchedAnalogValue(
  uint32_t objectInstance,
  uint32_t pollMs,
  uint32_t timeoutMs)
    : objectInstance_(objectInstance), pollMs_(pollMs), timeoutMs_(timeoutMs) {
  resetPreview("not read");
}

void BacnetDemoWatchedAnalogValue::setLogger(BacnetDemoLogCallback logger) {
  logger_ = logger;
}

void BacnetDemoWatchedAnalogValue::reset(const char* status) {
  preview_.presentValueSubscription.reset();
  preview_.statusSubscription.reset();
  session_ = nullptr;
  resetPreview(status);
}

bool BacnetDemoWatchedAnalogValue::setup(BacnetDeviceSession& session) {
  reset("pending");
  session_ = &session;
  BacnetProcessObject watched = watchedObject(session);
  preview_.configured = true;
  preview_.object = watched.objectId();
  readLabel(watched);
  readMetadata(watched);

  preview_.presentValueSubscription =
    subscribeProperty(watched, BacnetPropertyId::PresentValue, true);
  preview_.nextStatusRefreshMs = millis();

  updateFromCache();
  const bool active = preview_.presentValueSubscription &&
                      preview_.presentValueSubscription->active();
  String message = "watched ";
  message += bacnetObjectDisplayName(watched.objectId());
  message += " configured label=";
  message += preview_.label;
  message += " fallbackMs=";
  message += String(static_cast<unsigned long>(pollMs_));
  message += " active=";
  message += active ? "yes" : "no";
  log(BacnetDemoLogLevel::Info, message);
  return active;
}

void BacnetDemoWatchedAnalogValue::poll(BacnetDeviceSession& session,
                                        uint32_t now) {
  session_ = &session;
  if (preview_.presentValueSubscription) {
    session.poll(*preview_.presentValueSubscription, now);
  }

  if (preview_.statusSubscription) {
    session.poll(*preview_.statusSubscription, now);
    if (preview_.statusRefreshComplete &&
        !preview_.statusSubscription->inFlight()) {
      preview_.statusSubscription.reset();
      preview_.nextStatusPropertyIndex =
        (preview_.nextStatusPropertyIndex + 1) %
        (sizeof(kWatchedAnalogStatusProperties) /
         sizeof(kWatchedAnalogStatusProperties[0]));
      preview_.nextStatusRefreshMs = now + pollMs_;
    }
    return;
  }

  startStatusRefresh(session, now);
}

String BacnetDemoWatchedAnalogValue::objectSummary() const {
  if (session_ == nullptr) {
    return "No device selected";
  }
  return bacnetObjectDisplayName(preview_.object);
}

String BacnetDemoWatchedAnalogValue::labelSummary() const {
  if (session_ == nullptr) {
    return "No device selected";
  }
  return preview_.label.length() ? preview_.label : objectSummary();
}

String BacnetDemoWatchedAnalogValue::valueSummary() const {
  if (session_ == nullptr) {
    return "No device selected";
  }
  return preview_.hasPresentValue ? preview_.presentValue : "--";
}

String BacnetDemoWatchedAnalogValue::engineeringUnitSummary() const {
  if (session_ == nullptr) {
    return "No device selected";
  }
  if (!preview_.hasEngineeringUnit) {
    return metadataStatusText(preview_.engineeringUnitStatus);
  }

  String text = "unit=";
  text += String(preview_.engineeringUnitId);
  if (preview_.engineeringUnitSymbol.length() > 0) {
    text += " (";
    text += preview_.engineeringUnitSymbol;
    text += ")";
  } else if (bacnetEngineeringUnitSymbol(preview_.engineeringUnitId) != nullptr) {
    text += " (no units)";
  }
  return text;
}

String BacnetDemoWatchedAnalogValue::minMaxSummary() const {
  if (session_ == nullptr) {
    return "No device selected";
  }

  String text = metadataValueCore(preview_.minPresentValue);
  text += " ... ";
  text += metadataValueCore(preview_.maxPresentValue);
  if (preview_.minPresentValue.hasValue || preview_.maxPresentValue.hasValue) {
    appendUnitSuffix(text);
  }
  return text;
}

String BacnetDemoWatchedAnalogValue::resolutionSummary() const {
  if (session_ == nullptr) {
    return "No device selected";
  }
  return metadataValueText(preview_.resolution);
}

String BacnetDemoWatchedAnalogValue::covIncrementSummary() const {
  if (session_ == nullptr) {
    return "No device selected";
  }
  return metadataValueText(preview_.covIncrement);
}

String BacnetDemoWatchedAnalogValue::readStatusSummary() const {
  if (session_ == nullptr) {
    return "No device selected";
  }

  String summary = "present-value=";
  summary += preview_.presentValueStatus;
  summary += "\nunit=";
  summary += bacnetPropertyReadStatusText(preview_.engineeringUnitStatus);
  summary += " min=";
  summary += bacnetPropertyReadStatusText(preview_.minPresentValue.status);
  summary += " max=";
  summary += bacnetPropertyReadStatusText(preview_.maxPresentValue.status);
  summary += "\nresolution=";
  summary += bacnetPropertyReadStatusText(preview_.resolution.status);
  summary += " cov=";
  summary += bacnetPropertyReadStatusText(preview_.covIncrement.status);
  summary += "\nlast-refresh=";
  summary += preview_.lastStatus;
  return summary;
}

String BacnetDemoWatchedAnalogValue::alarmStateSummary() const {
  if (session_ == nullptr) {
    return "No device selected";
  }
  return preview_.alarmState;
}

String BacnetDemoWatchedAnalogValue::statusSummary() const {
  if (session_ == nullptr) {
    return "No device selected";
  }

  String summary = "pv-read-status=";
  summary += preview_.presentValueStatus;
  summary += "\nlast-refresh=";
  summary += preview_.lastStatus;
  summary += "\nalarm-state=";
  summary += preview_.alarmState;
  summary += "\nstate=";
  summary += preview_.state;
  summary += " flags=";
  summary += preview_.flags;
  summary += "\nevent-state=";
  summary += preview_.eventState;
  summary += "\nreliability=";
  summary += preview_.reliability;
  summary += " oos=";
  summary += preview_.outOfService;
  return summary;
}

String BacnetDemoWatchedAnalogValue::lastAttemptAgeSummary() const {
  if (session_ == nullptr) {
    return "No device selected";
  }
  return ageSummary(preview_.lastAttemptMs);
}

String BacnetDemoWatchedAnalogValue::lastSuccessAgeSummary() const {
  if (session_ == nullptr) {
    return "No device selected";
  }
  return preview_.hasPresentValue ? ageSummary(preview_.lastSuccessMs)
                                  : "no cached value";
}

void BacnetDemoWatchedAnalogValue::onSubscriptionUpdate(
  const BacnetSubscriptionNotification& notification) {
  auto* watched =
    static_cast<BacnetDemoWatchedAnalogValue*>(notification.userData);
  if (watched == nullptr) {
    return;
  }
  watched->handleSubscriptionUpdate(notification);
}

BacnetProcessObject BacnetDemoWatchedAnalogValue::watchedObject(
  BacnetDeviceSession& session) const {
  return session.analogValue(objectInstance_);
}

void BacnetDemoWatchedAnalogValue::resetPreview(const char* status) {
  preview_ = Preview{};
  preview_.object = BacnetObjectId{
    static_cast<uint16_t>(BacnetObjectType::AnalogValue), objectInstance_};
  preview_.label = "AV" + String(objectInstance_);
  preview_.presentValueStatus = status != nullptr ? status : "not read";
  preview_.lastStatus = preview_.presentValueStatus;
}

void BacnetDemoWatchedAnalogValue::log(BacnetDemoLogLevel level,
                                       const String& message) const {
  if (logger_ != nullptr) {
    logger_(level, message.c_str());
  }
}

void BacnetDemoWatchedAnalogValue::handleSubscriptionUpdate(
  const BacnetSubscriptionNotification& notification) {
  if (session_ == nullptr) {
    return;
  }

  preview_.lastStatus = bacnetPropertyDisplayName(notification.propertyId);
  preview_.lastStatus += "=";
  preview_.lastStatus += bacnetReadStatusText(notification.status);
  updateFromCache();

  if (notification.propertyId != BacnetPropertyId::PresentValue) {
    preview_.statusRefreshComplete = true;
  }

  if (notification.status != BacnetDeviceSessionReadStatus::Ack &&
      (notification.firstValue || notification.statusChanged)) {
    String message = "watched ";
    message += bacnetObjectDisplayName(notification.objectId);
    message += " ";
    message += bacnetPropertyDisplayName(notification.propertyId);
    message += " status=";
    message += bacnetReadStatusText(notification.status);
    log(BacnetDemoLogLevel::Warn, message);
  }
}

void BacnetDemoWatchedAnalogValue::appendUnitSuffix(String& text) const {
  if (preview_.engineeringUnitKnown &&
      preview_.engineeringUnitSymbol.length() > 0) {
    text += " ";
    text += preview_.engineeringUnitSymbol;
  }
}

String BacnetDemoWatchedAnalogValue::metadataValueCore(
  const Preview::MetadataField& field) const {
  if (field.hasValue) {
    return field.value.displayText();
  }
  return metadataStatusText(field.status);
}

String BacnetDemoWatchedAnalogValue::metadataValueText(
  const Preview::MetadataField& field) const {
  String text = metadataValueCore(field);
  if (field.hasValue) {
    appendUnitSuffix(text);
  }
  return text;
}

String BacnetDemoWatchedAnalogValue::metadataStatusText(
  BacnetPropertyReadStatus status) const {
  if (status == BacnetPropertyReadStatus::UnsupportedProperty ||
      status == BacnetPropertyReadStatus::ArrayIndexNotSupported ||
      status == BacnetPropertyReadStatus::UnsupportedDatatype) {
    return "unsupported";
  }
  if (status == BacnetPropertyReadStatus::Skipped) {
    return "--";
  }
  if (status == BacnetPropertyReadStatus::Ack) {
    return "--";
  }
  return "--";
}

void BacnetDemoWatchedAnalogValue::updateEngineeringUnit(
  BacnetPropertyReadStatus status,
  const BacnetValue& value,
  bool hasValue) {
  preview_.engineeringUnitStatus = status;
  if (!hasValue) {
    preview_.engineeringUnitKnown = false;
    preview_.hasEngineeringUnit = false;
    preview_.engineeringUnitId = 0;
    preview_.engineeringUnitSymbol = "";
    return;
  }

  uint32_t unitId = 0;
  if (!bacnetEngineeringUnitId(value, unitId)) {
    preview_.engineeringUnitKnown = false;
    preview_.hasEngineeringUnit = false;
    preview_.engineeringUnitId = 0;
    preview_.engineeringUnitSymbol = "";
    return;
  }

  preview_.engineeringUnitKnown = true;
  preview_.hasEngineeringUnit = true;
  preview_.engineeringUnitId = unitId;
  const char* symbol = bacnetEngineeringUnitSymbol(unitId);
  preview_.engineeringUnitSymbol = symbol != nullptr ? symbol : "";
}

void BacnetDemoWatchedAnalogValue::updateMetadataField(
  Preview::MetadataField& field,
  BacnetProcessObject watched,
  BacnetPropertyId propertyId,
  BacnetPropertyReadStatus status) {
  const BacnetProperty property = watched.property(propertyId);
  field.status = status;
  field.hasValue = property.hasValue();
  if (field.hasValue) {
    field.value = property.lastValue();
  }
}

String BacnetDemoWatchedAnalogValue::cachedValueWithUnit(
  const BacnetValue& value,
  BacnetPropertyReadStatus status,
  bool hasValue) const {
  if (!hasValue) {
    return "--";
  }

  String text = value.displayText();
  appendUnitSuffix(text);
  if (status != BacnetPropertyReadStatus::Ack) {
    text += " (stale: ";
    text += bacnetPropertyReadStatusText(status);
    text += ")";
  }
  return text;
}

String BacnetDemoWatchedAnalogValue::alarmStateFromStatus(
  const BacnetObjectStatus& status,
  bool hasStatusFlags,
  bool hasEventState,
  bool hasOutOfService) const {
  if (status.outOfServiceStatus == BacnetPropertyReadStatus::Ack &&
      status.outOfService) {
    return "out-of-service";
  }
  if (status.outOfServiceStatus != BacnetPropertyReadStatus::Ack &&
      hasOutOfService && status.outOfService) {
    String text = "out-of-service (stale: ";
    text += bacnetPropertyReadStatusText(status.outOfServiceStatus);
    text += ")";
    return text;
  }
  if (status.eventStateStatus == BacnetPropertyReadStatus::Ack) {
    if (status.eventState == 0 &&
        status.statusFlagsStatus == BacnetPropertyReadStatus::Ack &&
        !status.statusFlags.inAlarm) {
      return "normal";
    }
    return bacnetEventStateText(status.eventState);
  }
  if (hasEventState) {
    String text = bacnetEventStateText(status.eventState);
    text += " (stale: ";
    text += bacnetPropertyReadStatusText(status.eventStateStatus);
    text += ")";
    return text;
  }
  if (status.statusFlagsStatus == BacnetPropertyReadStatus::Ack &&
      status.statusFlags.inAlarm) {
    return "alarm";
  }
  if (status.statusFlagsStatus != BacnetPropertyReadStatus::Ack &&
      hasStatusFlags && status.statusFlags.inAlarm) {
    String text = "alarm (stale: ";
    text += bacnetPropertyReadStatusText(status.statusFlagsStatus);
    text += ")";
    return text;
  }
  return "unknown";
}

void BacnetDemoWatchedAnalogValue::updateFromCache() {
  if (session_ == nullptr) {
    return;
  }

  const BacnetProcessObject watched = watchedObject(*session_);
  BacnetObjectStatus status;
  status.objectId = watched.objectId();
  status.presentValueStatus = watched.presentValueStatus();
  status.presentValue = watched.presentValue();
  status.statusFlagsStatus = watched.statusFlagsStatus();
  status.statusFlags = watched.statusFlags();
  status.eventStateStatus = watched.eventStateStatus();
  status.eventState = watched.eventState();
  status.reliabilityStatus = watched.reliabilityStatus();
  status.reliability = watched.reliability();
  status.outOfServiceStatus = watched.outOfServiceStatus();
  status.outOfService = watched.outOfService();
  status.state = bacnetDeriveObjectHealthState(status, true);
  const bool hasStatusFlags =
    watched.property(BacnetPropertyId::StatusFlags).hasValue();
  const bool hasEventState =
    watched.property(BacnetPropertyId::EventState).hasValue();
  const bool hasOutOfService =
    watched.property(BacnetPropertyId::OutOfService).hasValue();

  preview_.configured = true;
  preview_.object = watched.objectId();
  preview_.hasPresentValue = watched.hasPresentValue();
  preview_.presentValue =
    cachedValueWithUnit(status.presentValue,
                        status.presentValueStatus,
                        preview_.hasPresentValue);
  preview_.presentValueStatus =
    bacnetPropertyReadStatusText(status.presentValueStatus);
  preview_.lastAttemptMs = watched.presentValueLastAttemptMs();
  preview_.lastSuccessMs = watched.presentValueLastSuccessMs();
  preview_.alarmState =
    alarmStateFromStatus(status, hasStatusFlags, hasEventState, hasOutOfService);
  preview_.state = bacnetObjectHealthStateText(status.state);
  preview_.flags = statusFlagsSummary(status);
  preview_.eventState =
    enumPropertySummary(status.eventStateStatus,
                        bacnetEventStateText(status.eventState));
  preview_.reliability =
    enumPropertySummary(status.reliabilityStatus,
                        bacnetReliabilityText(status.reliability));
  preview_.outOfService =
    boolPropertySummary(status.outOfServiceStatus, status.outOfService);
}

void BacnetDemoWatchedAnalogValue::readLabel(BacnetProcessObject watched) {
  BacnetValue value;
  BacnetDeviceSessionReadStatus status =
    watched.remoteObject().readObjectName(value, timeoutMs_);
  if (status == BacnetDeviceSessionReadStatus::Ack && value.textLength > 0) {
    preview_.label = value.displayText();
    return;
  }

  status = watched.remoteObject().readDescription(value, timeoutMs_);
  if (status == BacnetDeviceSessionReadStatus::Ack && value.textLength > 0) {
    preview_.label = value.displayText();
    return;
  }

  preview_.label = bacnetObjectDisplayName(watched.objectId());
}

BacnetPropertyReadStatus BacnetDemoWatchedAnalogValue::readMetadataProperty(
  BacnetProcessObject watched,
  BacnetPropertyId propertyId,
  BacnetValue& value) const {
  switch (propertyId) {
    case BacnetPropertyId::CovIncrement:
      watched.readCovIncrement(value, timeoutMs_);
      break;
    case BacnetPropertyId::MaxPresentValue:
      watched.readMaxPresentValue(value, timeoutMs_);
      break;
    case BacnetPropertyId::MinPresentValue:
      watched.readMinPresentValue(value, timeoutMs_);
      break;
    case BacnetPropertyId::Resolution:
      watched.readResolution(value, timeoutMs_);
      break;
    case BacnetPropertyId::Units:
      watched.readEngineeringUnits(value, timeoutMs_);
      break;
    default:
      watched.property(propertyId).read(value, timeoutMs_);
      break;
  }
  return watched.property(propertyId).lastStatus();
}

void BacnetDemoWatchedAnalogValue::readMetadata(BacnetProcessObject watched) {
  BacnetValue value;
  BacnetPropertyReadStatus status =
    readMetadataProperty(watched, BacnetPropertyId::Units, value);
  const BacnetProperty units = watched.property(BacnetPropertyId::Units);
  updateEngineeringUnit(status, units.lastValue(), units.hasValue());

  status =
    readMetadataProperty(watched, BacnetPropertyId::MinPresentValue, value);
  updateMetadataField(
    preview_.minPresentValue, watched, BacnetPropertyId::MinPresentValue, status);

  status =
    readMetadataProperty(watched, BacnetPropertyId::MaxPresentValue, value);
  updateMetadataField(
    preview_.maxPresentValue, watched, BacnetPropertyId::MaxPresentValue, status);

  status = readMetadataProperty(watched, BacnetPropertyId::Resolution, value);
  updateMetadataField(
    preview_.resolution, watched, BacnetPropertyId::Resolution, status);

  status = readMetadataProperty(watched, BacnetPropertyId::CovIncrement, value);
  updateMetadataField(
    preview_.covIncrement, watched, BacnetPropertyId::CovIncrement, status);
}

std::unique_ptr<BacnetPropertySubscription>
BacnetDemoWatchedAnalogValue::subscribeProperty(BacnetProcessObject watched,
                                                BacnetPropertyId propertyId,
                                                bool repeating) {
  BacnetSubscribeOptions options;
  options.fallbackPollMs = repeating ? pollMs_ : 0;
  options.timeoutMs = timeoutMs_;
  options.immediateFirstRead = true;
  options.notifyOnStatusChange = true;

  return std::unique_ptr<BacnetPropertySubscription>(
    new BacnetPropertySubscription(
      watched.property(propertyId)
        .subscribe(onSubscriptionUpdate, this, options)));
}

void BacnetDemoWatchedAnalogValue::startStatusRefresh(
  BacnetDeviceSession& session,
  uint32_t now) {
  if (!preview_.presentValueSubscription || preview_.statusSubscription ||
      now < preview_.nextStatusRefreshMs) {
    return;
  }

  if (preview_.nextStatusPropertyIndex >=
      (sizeof(kWatchedAnalogStatusProperties) /
       sizeof(kWatchedAnalogStatusProperties[0]))) {
    preview_.nextStatusPropertyIndex = 0;
  }

  const BacnetPropertyId propertyId =
    kWatchedAnalogStatusProperties[preview_.nextStatusPropertyIndex];
  BacnetProcessObject watched = watchedObject(session);
  preview_.statusRefreshProperty = propertyId;
  preview_.statusRefreshComplete = false;
  preview_.statusSubscription = subscribeProperty(watched, propertyId, false);
}
