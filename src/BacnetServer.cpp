// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetServer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr BacnetPropertyId kDeviceProperties[] = {
  BacnetPropertyId::ObjectIdentifier,
  BacnetPropertyId::ObjectName,
  BacnetPropertyId::ObjectType,
  BacnetPropertyId::SystemStatus,
  BacnetPropertyId::VendorName,
  BacnetPropertyId::VendorIdentifier,
  BacnetPropertyId::ModelName,
  BacnetPropertyId::FirmwareRevision,
  BacnetPropertyId::ApplicationSoftwareVersion,
  BacnetPropertyId::ProtocolVersion,
  BacnetPropertyId::ProtocolRevision,
  BacnetPropertyId::ProtocolServicesSupported,
  BacnetPropertyId::ProtocolObjectTypesSupported,
  BacnetPropertyId::ObjectList,
  BacnetPropertyId::MaxApduLengthAccepted,
  BacnetPropertyId::SegmentationSupported,
  BacnetPropertyId::ApduTimeout,
  BacnetPropertyId::NumberOfApduRetries,
  BacnetPropertyId::DeviceAddressBinding,
  BacnetPropertyId::DatabaseRevision,
  BacnetPropertyId::PropertyList,
};

constexpr BacnetPropertyId kAnalogValueProperties[] = {
  BacnetPropertyId::ObjectIdentifier,
  BacnetPropertyId::ObjectName,
  BacnetPropertyId::ObjectType,
  BacnetPropertyId::PresentValue,
  BacnetPropertyId::StatusFlags,
  BacnetPropertyId::EventState,
  BacnetPropertyId::OutOfService,
  BacnetPropertyId::Units,
  BacnetPropertyId::PropertyList,
};

constexpr BacnetPropertyId kAnalogInputProperties[] = {
  BacnetPropertyId::ObjectIdentifier,
  BacnetPropertyId::ObjectName,
  BacnetPropertyId::ObjectType,
  BacnetPropertyId::PresentValue,
  BacnetPropertyId::StatusFlags,
  BacnetPropertyId::EventState,
  BacnetPropertyId::OutOfService,
  BacnetPropertyId::Units,
  BacnetPropertyId::PropertyList,
};

constexpr BacnetPropertyId kBinaryInputProperties[] = {
  BacnetPropertyId::ObjectIdentifier,
  BacnetPropertyId::ObjectName,
  BacnetPropertyId::ObjectType,
  BacnetPropertyId::PresentValue,
  BacnetPropertyId::StatusFlags,
  BacnetPropertyId::EventState,
  BacnetPropertyId::OutOfService,
  BacnetPropertyId::PropertyList,
};

constexpr BacnetPropertyId kBinaryOutputProperties[] = {
  BacnetPropertyId::ObjectIdentifier,
  BacnetPropertyId::ObjectName,
  BacnetPropertyId::ObjectType,
  BacnetPropertyId::PresentValue,
  BacnetPropertyId::StatusFlags,
  BacnetPropertyId::EventState,
  BacnetPropertyId::OutOfService,
  BacnetPropertyId::Reliability,
  BacnetPropertyId::Polarity,
  BacnetPropertyId::PriorityArray,
  BacnetPropertyId::RelinquishDefault,
  BacnetPropertyId::PropertyList,
};

constexpr size_t kDevicePropertyCount = sizeof(kDeviceProperties) /
                                        sizeof(kDeviceProperties[0]);
constexpr size_t kAnalogValuePropertyCount = sizeof(kAnalogValueProperties) /
                                             sizeof(kAnalogValueProperties[0]);
constexpr size_t kAnalogInputPropertyCount = sizeof(kAnalogInputProperties) /
                                             sizeof(kAnalogInputProperties[0]);
constexpr size_t kBinaryInputPropertyCount = sizeof(kBinaryInputProperties) /
                                             sizeof(kBinaryInputProperties[0]);
constexpr size_t kBinaryOutputPropertyCount = sizeof(kBinaryOutputProperties) /
                                              sizeof(kBinaryOutputProperties[0]);

bool isBaseProperty(BacnetObjectId object, BacnetPropertyId property) {
  const BacnetPropertyId* properties = nullptr;
  size_t count = 0;
  if (object.type == static_cast<uint16_t>(BacnetObjectType::AnalogValue)) {
    properties = kAnalogValueProperties;
    count = kAnalogValuePropertyCount;
  } else if (object.type == static_cast<uint16_t>(BacnetObjectType::AnalogInput)) {
    properties = kAnalogInputProperties;
    count = kAnalogInputPropertyCount;
  } else if (object.type == static_cast<uint16_t>(BacnetObjectType::BinaryInput)) {
    properties = kBinaryInputProperties;
    count = kBinaryInputPropertyCount;
  } else if (object.type == static_cast<uint16_t>(BacnetObjectType::BinaryOutput)) {
    properties = kBinaryOutputProperties;
    count = kBinaryOutputPropertyCount;
  }
  for (size_t index = 0; index < count; ++index) {
    if (properties[index] == property)
      return true;
  }
  return false;
}

struct ObjectPropertyListContext {
  const BacnetServer* server = nullptr;
  BacnetObjectId object;
};

// cppcheck-suppress constParameterCallback
float readBoundFloat(void* context) {
  const auto* value = static_cast<const float*>(context);
  return value == nullptr ? 0.0F : *value;
}

// cppcheck-suppress constParameterCallback
bool readBoundBool(void* context) {
  const auto* value = static_cast<const bool*>(context);
  return value != nullptr && *value;
}

bool validObjectConfiguration(uint32_t instance, const char* objectName) {
  return instance <= 0x003FFFFFUL && objectName != nullptr &&
         std::strlen(objectName) < BacnetValue::kMaxTextLength;
}

bool hasExpectedPropertyType(BacnetPropertyId property,
                             BacnetObjectPropertyValueType type) {
  switch (property) {
    case BacnetPropertyId::Description:
      return type == BacnetObjectPropertyValueType::CharacterString;
    case BacnetPropertyId::MinPresentValue:
    case BacnetPropertyId::MaxPresentValue:
    case BacnetPropertyId::Resolution:
      return type == BacnetObjectPropertyValueType::Real ||
             type == BacnetObjectPropertyValueType::RealReference;
    case BacnetPropertyId::OutOfService:
      return type == BacnetObjectPropertyValueType::Boolean ||
             type == BacnetObjectPropertyValueType::BooleanReference;
    case BacnetPropertyId::Reliability:
    case BacnetPropertyId::EventState:
    case BacnetPropertyId::Units:
      return type == BacnetObjectPropertyValueType::Enumerated ||
             type == BacnetObjectPropertyValueType::EnumeratedReference;
    case BacnetPropertyId::StatusFlags:
      return type == BacnetObjectPropertyValueType::BitString ||
             type == BacnetObjectPropertyValueType::BitStringReference;
    default:
      return false;
  }
}

} // namespace

const char* bacnetObjectConfigurationStatusText(BacnetObjectConfigurationStatus status) {
  switch (status) {
    case BacnetObjectConfigurationStatus::Ok:
      return "ok";
    case BacnetObjectConfigurationStatus::InvalidArgument:
      return "invalid argument";
    case BacnetObjectConfigurationStatus::PropertyTypeMismatch:
      return "property type mismatch";
    case BacnetObjectConfigurationStatus::PropertyNotSupported:
      return "property not supported by object type";
    case BacnetObjectConfigurationStatus::DuplicateProperty:
      return "duplicate property";
    case BacnetObjectConfigurationStatus::PropertyCapacityExceeded:
      return "optional property capacity exceeded";
    case BacnetObjectConfigurationStatus::ServerCapacityExceeded:
      return "server object capacity exceeded";
    case BacnetObjectConfigurationStatus::RegistrationModeConflict:
      return "registration mode conflict";
  }
  return "unknown configuration error";
}

const char* bacnetPropertyIdText(BacnetPropertyId property) {
  switch (property) {
    case BacnetPropertyId::Description:
      return "Description";
    case BacnetPropertyId::EventState:
      return "Event_State";
    case BacnetPropertyId::MaxPresentValue:
      return "Max_Present_Value";
    case BacnetPropertyId::MinPresentValue:
      return "Min_Present_Value";
    case BacnetPropertyId::ObjectIdentifier:
      return "Object_Identifier";
    case BacnetPropertyId::PresentValue:
      return "Present_Value";
    case BacnetPropertyId::Reliability:
      return "Reliability";
    case BacnetPropertyId::Resolution:
      return "Resolution";
    case BacnetPropertyId::StatusFlags:
      return "Status_Flags";
    case BacnetPropertyId::Units:
      return "Units";
    default:
      return "BACnet property";
  }
}

void BacnetObjectWithProperties::initializeProperties(
  BacnetObjectPropertySlot* slots,
  size_t capacity) {
  propertySlots_ = slots;
  propertyCapacity_ = capacity;
  propertyCount_ = 0;
  configurationStatus_ = BacnetObjectConfigurationStatus::Ok;
  configurationErrorProperty_ = BacnetPropertyId::ObjectIdentifier;
}

BacnetObjectConfigurationStatus BacnetObjectWithProperties::configurationStatus() const {
  return configurationStatus_;
}

bool BacnetObjectWithProperties::isConfigurationValid() const {
  return configurationStatus_ == BacnetObjectConfigurationStatus::Ok;
}

BacnetObjectConfigurationStatus BacnetObjectWithProperties::recordConfigurationStatus(
  BacnetObjectConfigurationStatus status,
  BacnetPropertyId property) {
  if (configurationStatus_ == BacnetObjectConfigurationStatus::Ok &&
      status != BacnetObjectConfigurationStatus::Ok) {
    configurationStatus_ = status;
    configurationErrorProperty_ = property;
  }
  return status;
}

BacnetObjectConfigurationError BacnetObjectWithProperties::configurationErrorFor(
  BacnetObjectId object,
  const char* objectName) const {
  return {configurationStatus_, object, configurationErrorProperty_, objectName};
}

BacnetObjectConfigurationStatus BacnetObjectWithProperties::addPropertyValue(
  BacnetPropertyId property,
  BacnetObjectPropertyValueType type,
  BacnetObjectPropertySlot::Value value) {
  if (!hasExpectedPropertyType(property, type)) {
    return recordConfigurationStatus(BacnetObjectConfigurationStatus::PropertyTypeMismatch,
                                     property);
  }
  if (!supportsProperty(property, type)) {
    return recordConfigurationStatus(BacnetObjectConfigurationStatus::PropertyNotSupported,
                                     property);
  }
  for (size_t index = 0; index < propertyCount_; ++index) {
    if (propertySlots_[index].property == property) {
      return recordConfigurationStatus(BacnetObjectConfigurationStatus::DuplicateProperty,
                                       property);
    }
  }
  if (propertySlots_ == nullptr || propertyCount_ == propertyCapacity_) {
    return recordConfigurationStatus(BacnetObjectConfigurationStatus::PropertyCapacityExceeded,
                                     property);
  }
  propertySlots_[propertyCount_].property = property;
  propertySlots_[propertyCount_].type = type;
  propertySlots_[propertyCount_].value = value;
  ++propertyCount_;
  return BacnetObjectConfigurationStatus::Ok;
}

bool BacnetObjectWithProperties::readStoredProperty(BacnetPropertyId property,
                                                    BacnetValue& value) const {
  for (size_t index = 0; index < propertyCount_; ++index) {
    const BacnetObjectPropertySlot& slot = propertySlots_[index];
    if (slot.property != property) {
      continue;
    }
    value = BacnetValue{};
    switch (slot.type) {
      case BacnetObjectPropertyValueType::CharacterString: {
        if (slot.value.text == nullptr) {
          return false;
        }
        const size_t length = std::strlen(slot.value.text);
        if (length >= sizeof(value.text)) {
          return false;
        }
        std::memcpy(value.text, slot.value.text, length + 1U);
        value.type = BacnetValueType::CharacterString;
        value.textLength = length;
        return true;
      }
      case BacnetObjectPropertyValueType::Real:
        value.realValue = slot.value.realValue;
        break;
      case BacnetObjectPropertyValueType::RealReference:
        if (slot.value.realReference == nullptr) {
          return false;
        }
        value.realValue = *slot.value.realReference;
        break;
      case BacnetObjectPropertyValueType::Boolean:
        value.booleanValue = slot.value.booleanValue;
        value.type = BacnetValueType::Boolean;
        return true;
      case BacnetObjectPropertyValueType::BooleanReference:
        if (slot.value.booleanReference == nullptr) {
          return false;
        }
        value.booleanValue = *slot.value.booleanReference;
        value.type = BacnetValueType::Boolean;
        return true;
      case BacnetObjectPropertyValueType::Enumerated:
        value.unsignedValue = slot.value.enumeratedValue.value;
        value.type = BacnetValueType::Enumerated;
        return true;
      case BacnetObjectPropertyValueType::EnumeratedReference:
        if (slot.value.enumeratedReference == nullptr) {
          return false;
        }
        value.unsignedValue = slot.value.enumeratedReference->value;
        value.type = BacnetValueType::Enumerated;
        return true;
      case BacnetObjectPropertyValueType::BitString:
        value.bitStringValue = slot.value.bitStringValue.value;
        value.bitStringBitCount = slot.value.bitStringValue.bitCount;
        value.type = BacnetValueType::BitString;
        return value.bitStringBitCount != 0;
      case BacnetObjectPropertyValueType::BitStringReference:
        if (slot.value.bitStringReference == nullptr) {
          return false;
        }
        value.bitStringValue = slot.value.bitStringReference->value;
        value.bitStringBitCount = slot.value.bitStringReference->bitCount;
        value.type = BacnetValueType::BitString;
        return value.bitStringBitCount != 0;
    }
    if (!std::isfinite(value.realValue)) {
      return false;
    }
    value.type = BacnetValueType::Real;
    return true;
  }
  return false;
}

bool BacnetObjectWithProperties::storedPropertyAt(size_t index,
                                                  BacnetPropertyId& property) const {
  if (index >= propertyCount_) {
    return false;
  }
  property = propertySlots_[index].property;
  return true;
}

size_t BacnetObjectWithProperties::storedPropertyCount() const {
  return propertyCount_;
}

BacnetAnalogInput::BacnetAnalogInput() {
  initializeProperties(properties_, kMaxOptionalProperties);
}

BacnetObjectConfigurationStatus BacnetAnalogInput::configure(uint32_t instanceValue,
                                                             const char* name) {
  if (!validObjectConfiguration(instanceValue, name)) {
    return recordConfigurationStatus(BacnetObjectConfigurationStatus::InvalidArgument,
                                     BacnetPropertyId::ObjectIdentifier);
  }
  instance = instanceValue;
  objectName = name;
  return BacnetObjectConfigurationStatus::Ok;
}

BacnetObjectConfigurationStatus BacnetAnalogInput::bindPresentValue(
  const float* value) {
  if (value == nullptr) {
    return recordConfigurationStatus(BacnetObjectConfigurationStatus::InvalidArgument,
                                     BacnetPropertyId::PresentValue);
  }
  presentValueProvider = readBoundFloat;
  presentValueContext = const_cast<float*>(value);
  return BacnetObjectConfigurationStatus::Ok;
}

void BacnetAnalogInput::setUnits(uint32_t engineeringUnits) {
  units = engineeringUnits;
}

BacnetObjectConfigurationStatus BacnetAnalogInput::addProperty(
  BacnetPropertyId property, const char* value) {
  BacnetObjectPropertySlot::Value slotValue;
  slotValue.text = value;
  return value == nullptr ? recordConfigurationStatus(BacnetObjectConfigurationStatus::InvalidArgument,
                                                      property)
                          : addPropertyValue(property,
                                             BacnetObjectPropertyValueType::CharacterString,
                                             slotValue);
}

BacnetObjectConfigurationStatus BacnetAnalogInput::addProperty(
  BacnetPropertyId property, float value) {
  BacnetObjectPropertySlot::Value slotValue;
  slotValue.realValue = value;
  return addPropertyValue(property, BacnetObjectPropertyValueType::Real, slotValue);
}

BacnetObjectConfigurationStatus BacnetAnalogInput::addProperty(
  BacnetPropertyId property, const float* value) {
  BacnetObjectPropertySlot::Value slotValue;
  slotValue.realReference = value;
  return value == nullptr ? recordConfigurationStatus(BacnetObjectConfigurationStatus::InvalidArgument,
                                                      property)
                          : addPropertyValue(property,
                                             BacnetObjectPropertyValueType::RealReference,
                                             slotValue);
}

BacnetObjectConfigurationStatus BacnetAnalogInput::addProperty(
  BacnetPropertyId property, BacnetEnumeratedValue value) {
  BacnetObjectPropertySlot::Value slotValue;
  slotValue.enumeratedValue = value;
  return addPropertyValue(property, BacnetObjectPropertyValueType::Enumerated, slotValue);
}

BacnetObjectConfigurationStatus BacnetAnalogInput::addProperty(
  BacnetPropertyId property, const BacnetEnumeratedValue* value) {
  BacnetObjectPropertySlot::Value slotValue;
  slotValue.enumeratedReference = value;
  return value == nullptr ? recordConfigurationStatus(BacnetObjectConfigurationStatus::InvalidArgument,
                                                      property)
                          : addPropertyValue(property,
                                             BacnetObjectPropertyValueType::EnumeratedReference,
                                             slotValue);
}

BacnetObjectConfigurationStatus BacnetAnalogInput::addProperty(
  BacnetPropertyId property, BacnetServerStatusFlags value) {
  BacnetObjectPropertySlot::Value slotValue;
  slotValue.bitStringValue = value;
  return addPropertyValue(property, BacnetObjectPropertyValueType::BitString, slotValue);
}

BacnetObjectConfigurationStatus BacnetAnalogInput::addProperty(
  BacnetPropertyId property, const BacnetServerStatusFlags* value) {
  BacnetObjectPropertySlot::Value slotValue;
  slotValue.bitStringReference = value;
  return value == nullptr ? recordConfigurationStatus(BacnetObjectConfigurationStatus::InvalidArgument,
                                                      property)
                          : addPropertyValue(property,
                                             BacnetObjectPropertyValueType::BitStringReference,
                                             slotValue);
}

BacnetObjectId BacnetAnalogInput::objectId() const {
  return {static_cast<uint16_t>(BacnetObjectType::AnalogInput), instance};
}

BacnetObjectConfigurationError BacnetAnalogInput::configurationError() const {
  return configurationErrorFor(objectId(), objectName);
}

size_t BacnetAnalogInput::optionalPropertyCount() const {
  return storedPropertyCount();
}
bool BacnetAnalogInput::optionalPropertyAt(size_t index, BacnetPropertyId& property) const {
  return storedPropertyAt(index, property);
}
bool BacnetAnalogInput::readOptionalProperty(BacnetPropertyId property, BacnetValue& value) const {
  return readStoredProperty(property, value);
}
bool BacnetAnalogInput::supportsProperty(BacnetPropertyId property,
                                         BacnetObjectPropertyValueType type) const {
  return (property == BacnetPropertyId::Description &&
          type == BacnetObjectPropertyValueType::CharacterString) ||
         ((property == BacnetPropertyId::MinPresentValue ||
           property == BacnetPropertyId::MaxPresentValue ||
           property == BacnetPropertyId::Resolution) &&
          (type == BacnetObjectPropertyValueType::Real ||
           type == BacnetObjectPropertyValueType::RealReference)) ||
         ((property == BacnetPropertyId::Reliability || property == BacnetPropertyId::EventState) &&
          (type == BacnetObjectPropertyValueType::Enumerated ||
           type == BacnetObjectPropertyValueType::EnumeratedReference)) ||
         (property == BacnetPropertyId::StatusFlags &&
          (type == BacnetObjectPropertyValueType::BitString ||
           type == BacnetObjectPropertyValueType::BitStringReference));
}

BacnetBinaryInput::BacnetBinaryInput() {
  initializeProperties(properties_, kMaxOptionalProperties);
}

BacnetObjectConfigurationStatus BacnetBinaryInput::configure(uint32_t instanceValue,
                                                             const char* name) {
  if (!validObjectConfiguration(instanceValue, name)) {
    return recordConfigurationStatus(BacnetObjectConfigurationStatus::InvalidArgument,
                                     BacnetPropertyId::ObjectIdentifier);
  }
  instance = instanceValue;
  objectName = name;
  return BacnetObjectConfigurationStatus::Ok;
}

BacnetObjectConfigurationStatus BacnetBinaryInput::bindPresentValue(
  const bool* value) {
  if (value == nullptr) {
    return recordConfigurationStatus(BacnetObjectConfigurationStatus::InvalidArgument,
                                     BacnetPropertyId::PresentValue);
  }
  presentValueProvider = readBoundBool;
  presentValueContext = const_cast<bool*>(value);
  return BacnetObjectConfigurationStatus::Ok;
}

BacnetObjectConfigurationStatus BacnetBinaryInput::addProperty(
  BacnetPropertyId property, const char* value) {
  BacnetObjectPropertySlot::Value slotValue;
  slotValue.text = value;
  return value == nullptr ? recordConfigurationStatus(BacnetObjectConfigurationStatus::InvalidArgument,
                                                      property)
                          : addPropertyValue(property,
                                             BacnetObjectPropertyValueType::CharacterString,
                                             slotValue);
}

BacnetObjectConfigurationStatus BacnetBinaryInput::addProperty(
  BacnetPropertyId property, BacnetEnumeratedValue value) {
  BacnetObjectPropertySlot::Value slotValue;
  slotValue.enumeratedValue = value;
  return addPropertyValue(property, BacnetObjectPropertyValueType::Enumerated, slotValue);
}

BacnetObjectConfigurationStatus BacnetBinaryInput::addProperty(
  BacnetPropertyId property, const BacnetEnumeratedValue* value) {
  BacnetObjectPropertySlot::Value slotValue;
  slotValue.enumeratedReference = value;
  return value == nullptr ? recordConfigurationStatus(BacnetObjectConfigurationStatus::InvalidArgument,
                                                      property)
                          : addPropertyValue(property,
                                             BacnetObjectPropertyValueType::EnumeratedReference,
                                             slotValue);
}

BacnetObjectConfigurationStatus BacnetBinaryInput::addProperty(
  BacnetPropertyId property, BacnetServerStatusFlags value) {
  BacnetObjectPropertySlot::Value slotValue;
  slotValue.bitStringValue = value;
  return addPropertyValue(property, BacnetObjectPropertyValueType::BitString, slotValue);
}

BacnetObjectConfigurationStatus BacnetBinaryInput::addProperty(
  BacnetPropertyId property, const BacnetServerStatusFlags* value) {
  BacnetObjectPropertySlot::Value slotValue;
  slotValue.bitStringReference = value;
  return value == nullptr ? recordConfigurationStatus(BacnetObjectConfigurationStatus::InvalidArgument,
                                                      property)
                          : addPropertyValue(property,
                                             BacnetObjectPropertyValueType::BitStringReference,
                                             slotValue);
}

BacnetObjectId BacnetBinaryInput::objectId() const {
  return {static_cast<uint16_t>(BacnetObjectType::BinaryInput), instance};
}

BacnetObjectConfigurationError BacnetBinaryInput::configurationError() const {
  return configurationErrorFor(objectId(), objectName);
}

size_t BacnetBinaryInput::optionalPropertyCount() const {
  return storedPropertyCount();
}
bool BacnetBinaryInput::optionalPropertyAt(size_t index, BacnetPropertyId& property) const {
  return storedPropertyAt(index, property);
}
bool BacnetBinaryInput::readOptionalProperty(BacnetPropertyId property, BacnetValue& value) const {
  return readStoredProperty(property, value);
}
bool BacnetBinaryInput::supportsProperty(BacnetPropertyId property,
                                         BacnetObjectPropertyValueType type) const {
  return (property == BacnetPropertyId::Description &&
          type == BacnetObjectPropertyValueType::CharacterString) ||
         ((property == BacnetPropertyId::Reliability || property == BacnetPropertyId::EventState) &&
          (type == BacnetObjectPropertyValueType::Enumerated ||
           type == BacnetObjectPropertyValueType::EnumeratedReference)) ||
         (property == BacnetPropertyId::StatusFlags &&
          (type == BacnetObjectPropertyValueType::BitString ||
           type == BacnetObjectPropertyValueType::BitStringReference));
}

BacnetBinaryOutput::BacnetBinaryOutput() {
  initializeProperties(properties_, kMaxOptionalProperties);
}

BacnetObjectConfigurationStatus BacnetBinaryOutput::configure(uint32_t instanceValue,
                                                              const char* name) {
  if (!validObjectConfiguration(instanceValue, name)) {
    return recordConfigurationStatus(BacnetObjectConfigurationStatus::InvalidArgument,
                                     BacnetPropertyId::ObjectIdentifier);
  }
  instance = instanceValue;
  objectName = name;
  return BacnetObjectConfigurationStatus::Ok;
}

void BacnetBinaryOutput::setRelinquishDefault(bool value) {
  const bool previousEffectiveValue = priority.effectiveValue();
  priority.relinquishDefault = value;
  if (apply != nullptr && previousEffectiveValue != priority.effectiveValue()) {
    apply(applyContext, priority.effectiveValue(), outOfService);
  }
}

bool BacnetBinaryOutput::setLocalWritePriority(uint8_t value) {
  if (value == 0 || value > BacnetCommandPriority<bool>::kSlotCount) {
    return false;
  }
  localWritePriority = value;
  hasLocalWritePriority = true;
  return true;
}

void BacnetBinaryOutput::bindServerLocalWritePriority(const uint8_t* priorityValue) {
  serverLocalWritePriority_ = priorityValue;
}

bool BacnetBinaryOutput::applyLocalWrite(bool value,
                                         uint8_t priorityValue,
                                         bool relinquishValue) {
  if (priorityValue > BacnetCommandPriority<bool>::kSlotCount ||
      (priorityValue == 0 && relinquishValue)) {
    return false;
  }
  const bool previousEffectiveValue = priority.effectiveValue();
  if (priorityValue == 0) {
    priority.relinquishDefault = value;
  } else if (!priority.write(priorityValue, relinquishValue, value)) {
    return false;
  }
  if (apply != nullptr && previousEffectiveValue != priority.effectiveValue()) {
    apply(applyContext, priority.effectiveValue(), outOfService);
  }
  return true;
}

bool BacnetBinaryOutput::writeValue(bool value) {
  const uint8_t priorityValue = hasLocalWritePriority
                                  ? localWritePriority
                                  : (serverLocalWritePriority_ == nullptr
                                       ? BacnetCommandPriority<bool>::kSlotCount
                                       : *serverLocalWritePriority_);
  return applyLocalWrite(value, priorityValue, false);
}

bool BacnetBinaryOutput::writeValue(bool value, uint8_t priorityValue) {
  return applyLocalWrite(value, priorityValue, false);
}

bool BacnetBinaryOutput::relinquish(uint8_t priorityValue) {
  return priorityValue != 0 && applyLocalWrite(false, priorityValue, true);
}

void BacnetBinaryOutput::attachOutput(BacnetServerBinaryOutputApply outputApply,
                                      void* context) {
  apply = outputApply;
  applyContext = context;
}

BacnetObjectConfigurationStatus BacnetBinaryOutput::addProperty(
  BacnetPropertyId property, const char* value) {
  BacnetObjectPropertySlot::Value slotValue;
  slotValue.text = value;
  return value == nullptr ? recordConfigurationStatus(BacnetObjectConfigurationStatus::InvalidArgument,
                                                      property)
                          : addPropertyValue(property,
                                             BacnetObjectPropertyValueType::CharacterString,
                                             slotValue);
}

BacnetObjectId BacnetBinaryOutput::objectId() const {
  return {static_cast<uint16_t>(BacnetObjectType::BinaryOutput), instance};
}

BacnetObjectConfigurationError BacnetBinaryOutput::configurationError() const {
  return configurationErrorFor(objectId(), objectName);
}

size_t BacnetBinaryOutput::optionalPropertyCount() const {
  return storedPropertyCount();
}
bool BacnetBinaryOutput::optionalPropertyAt(size_t index, BacnetPropertyId& property) const {
  return storedPropertyAt(index, property);
}
bool BacnetBinaryOutput::readOptionalProperty(BacnetPropertyId property, BacnetValue& value) const {
  return readStoredProperty(property, value);
}
bool BacnetBinaryOutput::supportsProperty(BacnetPropertyId property,
                                          BacnetObjectPropertyValueType type) const {
  return property == BacnetPropertyId::Description &&
         type == BacnetObjectPropertyValueType::CharacterString;
}

BacnetServer::BacnetServer(BacnetDatagramTransport& transport)
    : transport_(&transport) {}

bool BacnetServer::setTransport(BacnetDatagramTransport& transport) {
  if (running_) {
    return false;
  }
  transport_ = &transport;
  return true;
}

bool BacnetServer::begin(const BacnetServerDevice& configuredDevice,
                         uint16_t localPort) {
  if (transport_ == nullptr ||
      configuredDevice.deviceInstance > 0x003FFFFFUL ||
      !transport_->begin(localPort)) {
    return false;
  }

  device_ = configuredDevice;
  port_ = localPort;
  for (size_t index = 0; index < kMaxCovSubscriptions; ++index) {
    covSubscriptions_[index] = BacnetServerCovSubscription{};
    for (size_t propertyIndex = 0; propertyIndex < kCovObjectPropertyCount; ++propertyIndex) {
      covSnapshotValid_[index][propertyIndex] = false;
    }
    covConfirmedRetryCounts_[index] = 0;
  }
  running_ = true;
  return true;
}

bool BacnetServer::begin(uint32_t deviceInstanceValue, uint16_t localPort) {
  BacnetServerDevice configuredDevice;
  configuredDevice.deviceInstance = deviceInstanceValue;
  return begin(configuredDevice, localPort);
}

void BacnetServer::end() {
  if (running_ && transport_ != nullptr) {
    transport_->end();
  }
  running_ = false;
  for (size_t index = 0; index < kMaxCovSubscriptions; ++index) {
    covSubscriptions_[index] = BacnetServerCovSubscription{};
    for (size_t propertyIndex = 0; propertyIndex < kCovObjectPropertyCount; ++propertyIndex) {
      covSnapshotValid_[index][propertyIndex] = false;
    }
    covConfirmedRetryCounts_[index] = 0;
  }
}

bool BacnetServer::setAnalogValues(BacnetServerAnalogValue* analogValues,
                                   size_t count) {
  if (count == 0) {
    analogValues_ = nullptr;
    analogValueCount_ = 0;
    return true;
  }
  if (analogValues == nullptr) {
    return false;
  }

  for (size_t index = 0; index < count; ++index) {
    const BacnetServerAnalogValue& analogValue = analogValues[index];
    if (analogValue.instance > 0x003FFFFFUL || analogValue.objectName == nullptr ||
        std::strlen(analogValue.objectName) >= BacnetValue::kMaxTextLength) {
      return false;
    }
    for (size_t previous = 0; previous < index; ++previous) {
      if (analogValues[previous].instance == analogValue.instance) {
        return false;
      }
    }
  }

  analogValues_ = analogValues;
  analogValueCount_ = count;
  return true;
}

size_t BacnetServer::analogValueCount() const {
  return analogValueCount_;
}

bool BacnetServer::setAnalogInputs(BacnetServerAnalogInput* analogInputs,
                                   size_t count) {
  if (objectCentricAnalogInputCount_ != 0) {
    return false;
  }
  if (count == 0) {
    analogInputs_ = nullptr;
    analogInputCount_ = 0;
    return true;
  }
  if (analogInputs == nullptr)
    return false;
  for (size_t index = 0; index < count; ++index) {
    const BacnetServerAnalogInput& input = analogInputs[index];
    if (input.instance > 0x003FFFFFUL || input.objectName == nullptr ||
        std::strlen(input.objectName) >= BacnetValue::kMaxTextLength) {
      return false;
    }
    for (size_t previous = 0; previous < index; ++previous) {
      if (analogInputs[previous].instance == input.instance)
        return false;
    }
  }
  analogInputs_ = analogInputs;
  analogInputCount_ = count;
  return true;
}

size_t BacnetServer::analogInputCount() const {
  return objectCentricAnalogInputCount_ != 0 ? objectCentricAnalogInputCount_
                                             : analogInputCount_;
}

bool BacnetServer::setBinaryInputs(BacnetServerBinaryInput* binaryInputs,
                                   size_t count) {
  if (objectCentricBinaryInputCount_ != 0) {
    return false;
  }
  if (count == 0) {
    binaryInputs_ = nullptr;
    binaryInputCount_ = 0;
    return true;
  }
  if (binaryInputs == nullptr)
    return false;
  for (size_t index = 0; index < count; ++index) {
    const BacnetServerBinaryInput& input = binaryInputs[index];
    if (input.instance > 0x003FFFFFUL || input.objectName == nullptr ||
        std::strlen(input.objectName) >= BacnetValue::kMaxTextLength) {
      return false;
    }
    for (size_t previous = 0; previous < index; ++previous) {
      if (binaryInputs[previous].instance == input.instance)
        return false;
    }
  }
  binaryInputs_ = binaryInputs;
  binaryInputCount_ = count;
  return true;
}

size_t BacnetServer::binaryInputCount() const {
  return objectCentricBinaryInputCount_ != 0 ? objectCentricBinaryInputCount_
                                             : binaryInputCount_;
}

bool BacnetServer::setBinaryOutputs(BacnetServerBinaryOutput* binaryOutputs,
                                    size_t count) {
  if (objectCentricBinaryOutputCount_ != 0) {
    return false;
  }
  if (count == 0) {
    binaryOutputs_ = nullptr;
    binaryOutputCount_ = 0;
    return true;
  }
  if (binaryOutputs == nullptr)
    return false;
  for (size_t index = 0; index < count; ++index) {
    const BacnetServerBinaryOutput& output = binaryOutputs[index];
    if (output.instance > 0x003FFFFFUL || output.objectName == nullptr ||
        std::strlen(output.objectName) >= BacnetValue::kMaxTextLength ||
        output.polarity > 1U) {
      return false;
    }
    for (size_t previous = 0; previous < index; ++previous) {
      if (binaryOutputs[previous].instance == output.instance)
        return false;
    }
  }
  binaryOutputs_ = binaryOutputs;
  binaryOutputCount_ = count;
  return true;
}

size_t BacnetServer::binaryOutputCount() const {
  return objectCentricBinaryOutputCount_ != 0 ? objectCentricBinaryOutputCount_
                                              : binaryOutputCount_;
}

BacnetObjectConfigurationStatus BacnetServer::addObject(BacnetAnalogInput& object) {
  if (!object.isConfigurationValid()) {
    return object.configurationStatus();
  }
  if (analogInputCount_ != 0) {
    return BacnetObjectConfigurationStatus::RegistrationModeConflict;
  }
  if (!validObjectConfiguration(object.instance, object.objectName)) {
    return BacnetObjectConfigurationStatus::InvalidArgument;
  }
  for (size_t index = 0; index < objectCentricAnalogInputCount_; ++index) {
    if (objectCentricAnalogInputs_[index]->instance == object.instance) {
      return BacnetObjectConfigurationStatus::DuplicateProperty;
    }
  }
  if (objectCentricAnalogInputCount_ == kMaxObjectCentricAnalogInputs) {
    return BacnetObjectConfigurationStatus::ServerCapacityExceeded;
  }
  objectCentricAnalogInputs_[objectCentricAnalogInputCount_] = &object;
  objectCentricAnalogInputProperties_[objectCentricAnalogInputCount_] = &object;
  ++objectCentricAnalogInputCount_;
  return BacnetObjectConfigurationStatus::Ok;
}

BacnetObjectConfigurationStatus BacnetServer::addObject(BacnetBinaryInput& object) {
  if (!object.isConfigurationValid()) {
    return object.configurationStatus();
  }
  if (binaryInputCount_ != 0) {
    return BacnetObjectConfigurationStatus::RegistrationModeConflict;
  }
  if (!validObjectConfiguration(object.instance, object.objectName)) {
    return BacnetObjectConfigurationStatus::InvalidArgument;
  }
  for (size_t index = 0; index < objectCentricBinaryInputCount_; ++index) {
    if (objectCentricBinaryInputs_[index]->instance == object.instance) {
      return BacnetObjectConfigurationStatus::DuplicateProperty;
    }
  }
  if (objectCentricBinaryInputCount_ == kMaxObjectCentricBinaryInputs) {
    return BacnetObjectConfigurationStatus::ServerCapacityExceeded;
  }
  objectCentricBinaryInputs_[objectCentricBinaryInputCount_] = &object;
  objectCentricBinaryInputProperties_[objectCentricBinaryInputCount_] = &object;
  ++objectCentricBinaryInputCount_;
  return BacnetObjectConfigurationStatus::Ok;
}

BacnetObjectConfigurationStatus BacnetServer::addObject(BacnetBinaryOutput& object) {
  if (!object.isConfigurationValid()) {
    return object.configurationStatus();
  }
  if (binaryOutputCount_ != 0) {
    return BacnetObjectConfigurationStatus::RegistrationModeConflict;
  }
  if (!validObjectConfiguration(object.instance, object.objectName) || object.polarity > 1U) {
    return BacnetObjectConfigurationStatus::InvalidArgument;
  }
  for (size_t index = 0; index < objectCentricBinaryOutputCount_; ++index) {
    if (objectCentricBinaryOutputs_[index]->instance == object.instance) {
      return BacnetObjectConfigurationStatus::DuplicateProperty;
    }
  }
  if (objectCentricBinaryOutputCount_ == kMaxObjectCentricBinaryOutputs) {
    return BacnetObjectConfigurationStatus::ServerCapacityExceeded;
  }
  objectCentricBinaryOutputs_[objectCentricBinaryOutputCount_] = &object;
  objectCentricBinaryOutputProperties_[objectCentricBinaryOutputCount_] = &object;
  object.bindServerLocalWritePriority(&localWritePriority_);
  ++objectCentricBinaryOutputCount_;
  return BacnetObjectConfigurationStatus::Ok;
}

bool BacnetServer::setPropertyRegistrations(
  const BacnetServerPropertyRegistration* registrations,
  size_t count) {
  if (count == 0) {
    propertyRegistrations_ = nullptr;
    propertyRegistrationCount_ = 0;
    return true;
  }
  if (registrations == nullptr)
    return false;

  for (size_t index = 0; index < count; ++index) {
    const BacnetServerPropertyRegistration& registration = registrations[index];
    if (registration.provider == nullptr ||
        registration.object.instance > 0x003FFFFFUL ||
        registration.property == BacnetPropertyId::PropertyList) {
      return false;
    }
    for (size_t previous = 0; previous < index; ++previous) {
      const BacnetServerPropertyRegistration& existing = registrations[previous];
      if (existing.object.type == registration.object.type &&
          existing.object.instance == registration.object.instance &&
          existing.property == registration.property) {
        return false;
      }
    }
  }

  propertyRegistrations_ = registrations;
  propertyRegistrationCount_ = count;
  return true;
}

size_t BacnetServer::propertyRegistrationCount() const {
  return propertyRegistrationCount_;
}

bool BacnetServer::isRunning() const {
  return running_;
}

const BacnetServerDevice& BacnetServer::device() const {
  return device_;
}

uint32_t BacnetServer::deviceInstance() const {
  return device_.deviceInstance;
}

uint16_t BacnetServer::port() const {
  return port_;
}

bool BacnetServer::setLocalWritePriority(uint8_t priority) {
  if (priority == 0 || priority > BacnetCommandPriority<bool>::kSlotCount) {
    return false;
  }
  localWritePriority_ = priority;
  return true;
}

uint8_t BacnetServer::localWritePriority() const {
  return localWritePriority_;
}

void BacnetServer::setClock(const BacnetMonotonicClock* clock) {
  clock_ = clock;
}

size_t BacnetServer::covSubscriptionCount() const {
  return static_cast<size_t>(std::count_if(
    covSubscriptions_, covSubscriptions_ + kMaxCovSubscriptions, [](const BacnetServerCovSubscription& subscription) {
      return subscription.state != BacnetServerCovSubscriptionState::Inactive;
    }));
}

bool BacnetServer::covSubscriptionAt(size_t index,
                                     BacnetServerCovSubscription& subscription) const {
  size_t current = 0;
  for (const BacnetServerCovSubscription& candidate : covSubscriptions_) {
    if (candidate.state == BacnetServerCovSubscriptionState::Inactive) {
      continue;
    }
    if (current++ == index) {
      subscription = candidate;
      return true;
    }
  }
  return false;
}

uint32_t BacnetServer::nowMs() const {
  return clock_ == nullptr ? 0U : clock_->nowMs();
}

bool BacnetServer::endpointEquals(const BacnetIpEndpoint& left,
                                  const BacnetIpEndpoint& right) {
  return left.port == right.port &&
         left.address[0] == right.address[0] && left.address[1] == right.address[1] &&
         left.address[2] == right.address[2] && left.address[3] == right.address[3];
}

void BacnetServer::setActivityListener(BacnetServerActivityListener listener,
                                       void* context) {
  activityListener_ = listener;
  activityListenerContext_ = context;
}

BacnetServerPollResult BacnetServer::poll() {
  if (!running_ || transport_ == nullptr) {
    return BacnetServerPollResult::NotRunning;
  }

  uint8_t packet[kMaxDatagramSize] = {};
  BacnetIpEndpoint source;
  const size_t bytesRead = transport_->receive(packet, sizeof(packet), source);
  if (bytesRead == 0) {
    return processCovSubscriptions();
  }
  if (bytesRead > sizeof(packet)) {
    return BacnetServerPollResult::Malformed;
  }

  for (BacnetServerCovSubscription& subscription : covSubscriptions_) {
    if (subscription.state != BacnetServerCovSubscriptionState::AwaitingConfirmedAck ||
        !endpointEquals(subscription.peer, source)) {
      continue;
    }
    const BacnetSubscribeCovResponseKind response =
      BacnetProtocol::classifyConfirmedCovNotificationResponse(
        packet, bytesRead, subscription.pendingInvokeId);
    if (response == BacnetSubscribeCovResponseKind::Ack) {
      subscription.state = BacnetServerCovSubscriptionState::Active;
      subscription.lastAckMs = nowMs();
      covConfirmedRetryCounts_[static_cast<size_t>(&subscription - covSubscriptions_)] = 0;
      return BacnetServerPollResult::Ignored;
    }
    if (response == BacnetSubscribeCovResponseKind::Error ||
        response == BacnetSubscribeCovResponseKind::Reject ||
        response == BacnetSubscribeCovResponseKind::Abort) {
      subscription.state = BacnetServerCovSubscriptionState::Active;
      covConfirmedRetryCounts_[static_cast<size_t>(&subscription - covSubscriptions_)] = 0;
      return BacnetServerPollResult::Ignored;
    }
  }

  BacnetWhoIsRequest whoIs;
  if (BacnetProtocol::parseWhoIsRequest(packet, bytesRead, whoIs)) {
    if (!whoIs.includes(device_.deviceInstance)) {
      return BacnetServerPollResult::Ignored;
    }
    return sendIAm(source) ? BacnetServerPollResult::IAmSent
                           : BacnetServerPollResult::SendFailed;
  }

  BacnetConfirmedRequestHeader confirmedRequest;
  const auto confirmedStatus = BacnetProtocol::parseConfirmedRequestHeader(
    packet, bytesRead, confirmedRequest);
  if (confirmedStatus == BacnetConfirmedRequestParseStatus::Malformed) {
    return BacnetServerPollResult::Malformed;
  }
  if (confirmedStatus != BacnetConfirmedRequestParseStatus::Confirmed) {
    return BacnetServerPollResult::Ignored;
  }

  BacnetReadPropertyRequestHeader readProperty;
  const auto readStatus = BacnetProtocol::parseReadPropertyRequest(
    packet, bytesRead, readProperty);
  if (readStatus == BacnetReadPropertyRequestParseStatus::Malformed) {
    return BacnetServerPollResult::Malformed;
  }
  if (readStatus == BacnetReadPropertyRequestParseStatus::ReadProperty) {
    if (activityListener_ != nullptr) {
      activityListener_(activityListenerContext_, BacnetServerActivity{
                                                    source,
                                                    BacnetServerActivityService::ReadProperty,
                                                    readProperty.request.object,
                                                    readProperty.request.property,
                                                    false,
                                                    0,
                                                  });
    }
    return handleReadProperty(source, readProperty);
  }

  BacnetWritePropertyRequestHeader writeProperty;
  const auto writeStatus = BacnetProtocol::parseWritePropertyRequest(
    packet, bytesRead, writeProperty);
  if (writeStatus == BacnetWritePropertyRequestParseStatus::Malformed)
    return BacnetServerPollResult::Malformed;
  if (writeStatus == BacnetWritePropertyRequestParseStatus::WriteProperty) {
    if (activityListener_ != nullptr) {
      activityListener_(activityListenerContext_, BacnetServerActivity{
                                                    source,
                                                    BacnetServerActivityService::WriteProperty,
                                                    writeProperty.request.object,
                                                    writeProperty.request.property,
                                                    writeProperty.hasPriority,
                                                    writeProperty.priority,
                                                  });
    }
    return handleWriteProperty(source, writeProperty);
  }

  BacnetSubscribeCovRequestHeader subscribeCov;
  const auto subscribeStatus = BacnetProtocol::parseSubscribeCovRequest(
    packet, bytesRead, subscribeCov);
  if (subscribeStatus == BacnetSubscribeCovRequestParseStatus::Malformed) {
    return BacnetServerPollResult::Malformed;
  }
  if (subscribeStatus == BacnetSubscribeCovRequestParseStatus::SubscribeCov ||
      subscribeStatus == BacnetSubscribeCovRequestParseStatus::SubscribeCovProperty) {
    return handleSubscribeCov(source, subscribeCov);
  }

  return sendReject(source, confirmedRequest.invokeId)
           ? BacnetServerPollResult::RejectSent
           : BacnetServerPollResult::SendFailed;
}

BacnetServerPollResult BacnetServer::handleReadProperty(
  const BacnetIpEndpoint& source,
  const BacnetReadPropertyRequestHeader& request) {
  uint32_t errorClass = 0;
  uint32_t errorCode = 0;
  BacnetValue value;
  size_t responseSize = 0;
  const bool isDevice =
    request.request.object.type == static_cast<uint16_t>(BacnetObjectType::Device) &&
    request.request.object.instance == device_.deviceInstance;
  const BacnetServerAnalogValue* analogValue =
    request.request.object.type == static_cast<uint16_t>(BacnetObjectType::AnalogValue)
      ? findAnalogValue(request.request.object.instance)
      : nullptr;
  const BacnetServerAnalogInput* analogInput =
    request.request.object.type == static_cast<uint16_t>(BacnetObjectType::AnalogInput)
      ? findAnalogInput(request.request.object.instance)
      : nullptr;
  const BacnetServerBinaryInput* binaryInput =
    request.request.object.type == static_cast<uint16_t>(BacnetObjectType::BinaryInput)
      ? findBinaryInput(request.request.object.instance)
      : nullptr;
  const BacnetServerBinaryOutput* binaryOutput =
    request.request.object.type == static_cast<uint16_t>(BacnetObjectType::BinaryOutput)
      ? findBinaryOutput(request.request.object.instance)
      : nullptr;

  if (!isDevice && analogValue == nullptr && analogInput == nullptr && binaryInput == nullptr &&
      binaryOutput == nullptr) {
    errorClass = 1;
    errorCode = 31;
  } else if (isDevice) {
    if (request.request.property == BacnetPropertyId::ObjectList) {
      if (request.request.arrayIndex != kBacnetNoArrayIndex &&
          request.request.arrayIndex > analogValueCount_ + analogInputCount() + binaryInputCount() + binaryOutputCount() + 1U) {
        errorClass = 2;
        errorCode = 42;
      }
    } else if (request.request.property == BacnetPropertyId::PropertyList) {
      if (request.request.arrayIndex != kBacnetNoArrayIndex &&
          request.request.arrayIndex > kDevicePropertyCount) {
        errorClass = 2;
        errorCode = 42;
      }
    } else if (request.request.property == BacnetPropertyId::DeviceAddressBinding) {
      if (request.request.arrayIndex != kBacnetNoArrayIndex) {
        errorClass = 2;
        errorCode = 42;
      }
    } else if (request.request.arrayIndex != kBacnetNoArrayIndex) {
      errorClass = 2;
      errorCode = 42;
    } else if (!readDeviceProperty(request.request.property, value)) {
      errorClass = 2;
      errorCode = 32;
    }
  } else if (request.request.property == BacnetPropertyId::PropertyList) {
    if (request.request.arrayIndex != kBacnetNoArrayIndex &&
        request.request.arrayIndex > objectPropertyCount(request.request.object)) {
      errorClass = 2;
      errorCode = 42;
    }
  } else if (request.request.arrayIndex != kBacnetNoArrayIndex) {
    if (binaryOutput != nullptr && request.request.property == BacnetPropertyId::PriorityArray &&
        request.request.arrayIndex <= BacnetCommandPriority<bool>::kSlotCount) {
      // Priority_Array is the one Binary Output array exposed by this profile.
    } else {
      errorClass = 2;
      errorCode = 42;
    }
  } else if (!(binaryOutput != nullptr &&
               request.request.property == BacnetPropertyId::PriorityArray) &&
             ((analogValue != nullptr &&
               !readAnalogValueProperty(*analogValue, request.request.property, value)) ||
              (analogInput != nullptr &&
               !readAnalogInputProperty(*analogInput, request.request.property, value)) ||
              (binaryInput != nullptr &&
               !readBinaryInputProperty(*binaryInput, request.request.property, value)) ||
              (binaryOutput != nullptr &&
               !readBinaryOutputProperty(*binaryOutput, request.request.property, value)))) {
    errorClass = 2;
    errorCode = 32;
  }

  uint8_t response[kMaxDatagramSize] = {};
  if (errorCode == 0) {
    if (isDevice && request.request.property == BacnetPropertyId::ObjectList) {
      responseSize = BacnetProtocol::buildReadPropertyObjectListAck(
        response,
        sizeof(response),
        request,
        analogValueCount_ + analogInputCount() + binaryInputCount() + binaryOutputCount() + 1U,
        objectListEntry,
        this);
    } else if (isDevice && request.request.property == BacnetPropertyId::PropertyList) {
      responseSize = BacnetProtocol::buildReadPropertyPropertyListAck(
        response, sizeof(response), request, kDeviceProperties, kDevicePropertyCount);
    } else if (!isDevice && request.request.property == BacnetPropertyId::PropertyList) {
      const ObjectPropertyListContext context{this, request.request.object};
      responseSize = BacnetProtocol::buildReadPropertyPropertyListAck(
        response,
        sizeof(response),
        request,
        objectPropertyCount(request.request.object),
        objectPropertyEntry,
        &context);
    } else if (binaryOutput != nullptr &&
               request.request.property == BacnetPropertyId::PriorityArray) {
      responseSize = BacnetProtocol::buildReadPropertyPriorityArrayAck(
        response,
        sizeof(response),
        request,
        BacnetCommandPriority<bool>::kSlotCount,
        binaryOutputPriorityEntry,
        binaryOutput);
    } else if (isDevice && request.request.property == BacnetPropertyId::DeviceAddressBinding) {
      responseSize = BacnetProtocol::buildReadPropertyEmptyListAck(
        response, sizeof(response), request);
    } else {
      responseSize = BacnetProtocol::buildReadPropertyAck(
        response, sizeof(response), request, value);
    }
  } else {
    responseSize = BacnetProtocol::buildReadPropertyError(response, sizeof(response), request.invokeId, errorClass, errorCode);
  }
  if (responseSize == 0 || !transport_->send(source, response, responseSize)) {
    return BacnetServerPollResult::SendFailed;
  }
  return errorCode == 0 ? BacnetServerPollResult::ReadPropertyAckSent
                        : BacnetServerPollResult::ReadPropertyErrorSent;
}

BacnetServerPollResult BacnetServer::handleWriteProperty(
  const BacnetIpEndpoint& source,
  const BacnetWritePropertyRequestHeader& request) {
  uint32_t errorClass = 0;
  uint32_t errorCode = 0;
  BacnetServerBinaryOutput* output =
    request.request.object.type == static_cast<uint16_t>(BacnetObjectType::BinaryOutput)
      ? findBinaryOutput(request.request.object.instance)
      : nullptr;
  if (output == nullptr) {
    errorClass = 1;
    errorCode = 31;
  } else if (request.request.arrayIndex != kBacnetNoArrayIndex) {
    errorClass = 2;
    errorCode = 42;
  } else if (request.request.property == BacnetPropertyId::PresentValue) {
    const uint8_t priority = request.hasPriority ? request.priority : 16U;
    const bool relinquish = request.value.type == BacnetValueType::Null;
    const bool validValue = relinquish ||
                            (request.value.type == BacnetValueType::Enumerated && request.value.unsignedValue <= 1U);
    if (priority == 0 || priority > BacnetCommandPriority<bool>::kSlotCount) {
      errorClass = 2;
      errorCode = 37; // value-out-of-range
    } else if (!validValue) {
      errorClass = 2;
      errorCode = 9; // invalid-data-type
    } else {
      const bool previousEffectiveValue = output->priority.effectiveValue();
      const bool writeSucceeded = output->priority.write(priority,
                                                         relinquish,
                                                         request.value.unsignedValue == 1U);
      if (!writeSucceeded) {
        errorClass = 2;
        errorCode = 37;
      } else if (output->apply != nullptr &&
                 previousEffectiveValue != output->priority.effectiveValue()) {
        output->apply(output->applyContext, output->priority.effectiveValue(), output->outOfService);
      }
    }
  } else if (request.request.property == BacnetPropertyId::OutOfService) {
    if (request.hasPriority || request.value.type != BacnetValueType::Boolean) {
      errorClass = 2;
      errorCode = 9;
    } else {
      output->outOfService = request.value.booleanValue;
      if (output->apply != nullptr) {
        output->apply(output->applyContext, output->priority.effectiveValue(), output->outOfService);
      }
    }
  } else {
    errorClass = 2;
    errorCode = 40; // write-access-denied
  }

  uint8_t response[kMaxDatagramSize] = {};
  const size_t responseSize = errorCode == 0
                                ? BacnetProtocol::buildWritePropertyAck(response, sizeof(response), request.invokeId)
                                : BacnetProtocol::buildWritePropertyError(response, sizeof(response), request.invokeId, errorClass, errorCode);
  if (responseSize == 0 || !transport_->send(source, response, responseSize))
    return BacnetServerPollResult::SendFailed;
  return errorCode == 0 ? BacnetServerPollResult::WritePropertyAckSent
                        : BacnetServerPollResult::WritePropertyErrorSent;
}

bool BacnetServer::readCovValue(BacnetObjectId object,
                                BacnetPropertyId property,
                                uint32_t arrayIndex,
                                BacnetValue& value) const {
  if (arrayIndex != kBacnetNoArrayIndex) {
    return false;
  }
  if (object.type == static_cast<uint16_t>(BacnetObjectType::AnalogValue)) {
    const BacnetServerAnalogValue* input = findAnalogValue(object.instance);
    return input != nullptr && readAnalogValueProperty(*input, property, value);
  }
  if (object.type == static_cast<uint16_t>(BacnetObjectType::AnalogInput)) {
    const BacnetServerAnalogInput* input = findAnalogInput(object.instance);
    return input != nullptr && readAnalogInputProperty(*input, property, value);
  }
  if (object.type == static_cast<uint16_t>(BacnetObjectType::BinaryInput)) {
    const BacnetServerBinaryInput* input = findBinaryInput(object.instance);
    return input != nullptr && readBinaryInputProperty(*input, property, value);
  }
  if (object.type == static_cast<uint16_t>(BacnetObjectType::BinaryOutput)) {
    const BacnetServerBinaryOutput* output = findBinaryOutput(object.instance);
    return output != nullptr && readBinaryOutputProperty(*output, property, value);
  }
  return false;
}

bool BacnetServer::collectCovValues(const BacnetServerCovSubscription& subscription,
                                    BacnetCovPropertyValue* values,
                                    size_t capacity,
                                    size_t& count) const {
  count = 0;
  if (values == nullptr || capacity == 0) {
    return false;
  }
  if (subscription.isPropertySubscription) {
    values[0].property = subscription.property;
    values[0].arrayIndex = subscription.arrayIndex;
    if (!readCovValue(subscription.object, subscription.property, subscription.arrayIndex, values[0].value)) {
      return false;
    }
    count = 1;
    return true;
  }
  static constexpr BacnetPropertyId kObjectCovProperties[] = {
    BacnetPropertyId::PresentValue,
    BacnetPropertyId::StatusFlags,
  };
  if (capacity < kCovObjectPropertyCount) {
    return false;
  }
  for (size_t index = 0; index < kCovObjectPropertyCount; ++index) {
    values[index].property = kObjectCovProperties[index];
    values[index].arrayIndex = kBacnetNoArrayIndex;
    if (!readCovValue(subscription.object, values[index].property, values[index].arrayIndex, values[index].value)) {
      return false;
    }
  }
  count = kCovObjectPropertyCount;
  return true;
}

bool BacnetServer::covValuesChanged(const BacnetServerCovSubscription& subscription,
                                    const BacnetCovPropertyValue* values,
                                    size_t count) const {
  const size_t subscriptionIndex = static_cast<size_t>(&subscription - covSubscriptions_);
  if (subscriptionIndex >= kMaxCovSubscriptions || values == nullptr ||
      count == 0 || count > kCovObjectPropertyCount) {
    return true;
  }
  for (size_t index = 0; index < count; ++index) {
    const BacnetValue& value = values[index].value;
    if (!covSnapshotValid_[subscriptionIndex][index] ||
        covSnapshotTypes_[subscriptionIndex][index] != value.type) {
      return true;
    }
    switch (value.type) {
      case BacnetValueType::Boolean:
        if (covSnapshotBoolean_[subscriptionIndex][index] != value.booleanValue)
          return true;
        break;
      case BacnetValueType::Real:
        if (covSnapshotReal_[subscriptionIndex][index] != value.realValue) {
          if (!subscription.hasCovIncrement || index != 0U) {
            return true;
          }
          float difference = value.realValue - covSnapshotReal_[subscriptionIndex][index];
          if (difference < 0.0F) {
            difference = -difference;
          }
          if (difference >= subscription.covIncrement) {
            return true;
          }
        }
        break;
      case BacnetValueType::BitString:
        if (covSnapshotBitString_[subscriptionIndex][index] != value.bitStringValue ||
            covSnapshotBitCount_[subscriptionIndex][index] != value.bitStringBitCount)
          return true;
        break;
      case BacnetValueType::Unsigned:
      case BacnetValueType::Enumerated:
        if (covSnapshotUnsigned_[subscriptionIndex][index] != value.unsignedValue)
          return true;
        break;
      default:
        return true;
    }
  }
  return false;
}

void BacnetServer::storeCovValues(BacnetServerCovSubscription& subscription,
                                  const BacnetCovPropertyValue* values,
                                  size_t count) {
  const size_t subscriptionIndex = static_cast<size_t>(&subscription - covSubscriptions_);
  if (subscriptionIndex >= kMaxCovSubscriptions || values == nullptr) {
    return;
  }
  for (size_t index = 0; index < count && index < kCovObjectPropertyCount; ++index) {
    const BacnetValue& value = values[index].value;
    covSnapshotTypes_[subscriptionIndex][index] = value.type;
    covSnapshotBoolean_[subscriptionIndex][index] = value.booleanValue;
    covSnapshotUnsigned_[subscriptionIndex][index] = value.unsignedValue;
    covSnapshotReal_[subscriptionIndex][index] = value.realValue;
    covSnapshotBitString_[subscriptionIndex][index] = value.bitStringValue;
    covSnapshotBitCount_[subscriptionIndex][index] = value.bitStringBitCount;
    covSnapshotValid_[subscriptionIndex][index] = true;
  }
}

bool BacnetServer::sendCovNotification(BacnetServerCovSubscription& subscription,
                                       const BacnetCovPropertyValue* values,
                                       size_t valueCount,
                                       uint32_t now) {
  uint8_t notification[kMaxDatagramSize] = {};
  const uint32_t timeRemaining = subscription.lifetimeSeconds == 0U ||
                                     clock_ == nullptr
                                   ? 0U
                                   : (subscription.expiresAtMs - now + 999U) / 1000U;
  const uint8_t invokeId = subscription.confirmed ? nextCovInvokeId_++ : 0U;
  if (nextCovInvokeId_ == 0U) {
    nextCovInvokeId_ = 1U;
  }
  const size_t size = BacnetProtocol::buildCovNotification(
    notification, sizeof(notification), subscription.processId, BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::Device), device_.deviceInstance}, subscription.object, timeRemaining, values, valueCount, subscription.confirmed, invokeId);
  if (size == 0U || !transport_->send(subscription.peer, notification, size)) {
    return false;
  }
  subscription.lastSentMs = now;
  subscription.pendingInvokeId = invokeId;
  subscription.state = subscription.confirmed
                         ? BacnetServerCovSubscriptionState::AwaitingConfirmedAck
                         : BacnetServerCovSubscriptionState::Active;
  storeCovValues(subscription, values, valueCount);
  return true;
}

BacnetServerPollResult BacnetServer::processCovSubscriptions() {
  const uint32_t now = nowMs();
  for (size_t index = 0; index < kMaxCovSubscriptions; ++index) {
    BacnetServerCovSubscription& subscription = covSubscriptions_[index];
    if (subscription.state == BacnetServerCovSubscriptionState::Inactive) {
      continue;
    }
    if (subscription.lifetimeSeconds != 0U && clock_ != nullptr &&
        static_cast<int32_t>(now - subscription.expiresAtMs) >= 0) {
      subscription = BacnetServerCovSubscription{};
      for (size_t propertyIndex = 0; propertyIndex < kCovObjectPropertyCount; ++propertyIndex) {
        covSnapshotValid_[index][propertyIndex] = false;
      }
      covConfirmedRetryCounts_[index] = 0;
      continue;
    }
    if (subscription.state == BacnetServerCovSubscriptionState::AwaitingConfirmedAck) {
      if (clock_ != nullptr && now - subscription.lastSentMs >= device_.apduTimeout) {
        if (covConfirmedRetryCounts_[index] >= device_.numberOfApduRetries) {
          subscription.state = BacnetServerCovSubscriptionState::Active;
          covConfirmedRetryCounts_[index] = 0;
          continue;
        }
        BacnetCovPropertyValue retryValues[kCovObjectPropertyCount];
        const size_t retryCount = subscription.isPropertySubscription ? 1U : kCovObjectPropertyCount;
        for (size_t propertyIndex = 0; propertyIndex < retryCount; ++propertyIndex) {
          retryValues[propertyIndex].property = subscription.isPropertySubscription
                                                  ? subscription.property
                                                  : (propertyIndex == 0 ? BacnetPropertyId::PresentValue
                                                                        : BacnetPropertyId::StatusFlags);
          retryValues[propertyIndex].arrayIndex = subscription.isPropertySubscription
                                                    ? subscription.arrayIndex
                                                    : kBacnetNoArrayIndex;
          BacnetValue& retryValue = retryValues[propertyIndex].value;
          retryValue.type = covSnapshotTypes_[index][propertyIndex];
          retryValue.booleanValue = covSnapshotBoolean_[index][propertyIndex];
          retryValue.unsignedValue = covSnapshotUnsigned_[index][propertyIndex];
          retryValue.realValue = covSnapshotReal_[index][propertyIndex];
          retryValue.bitStringValue = covSnapshotBitString_[index][propertyIndex];
          retryValue.bitStringBitCount = covSnapshotBitCount_[index][propertyIndex];
        }
        ++covConfirmedRetryCounts_[index];
        return sendCovNotification(subscription, retryValues, retryCount, now)
                 ? BacnetServerPollResult::CovNotificationSent
                 : BacnetServerPollResult::SendFailed;
      }
      continue;
    }
    BacnetCovPropertyValue values[kCovObjectPropertyCount];
    size_t valueCount = 0;
    if (!collectCovValues(subscription, values, kCovObjectPropertyCount, valueCount)) {
      continue;
    }
    if (!covValuesChanged(subscription, values, valueCount)) {
      continue;
    }
    covConfirmedRetryCounts_[index] = 0;
    return sendCovNotification(subscription, values, valueCount, now)
             ? BacnetServerPollResult::CovNotificationSent
             : BacnetServerPollResult::SendFailed;
  }
  return BacnetServerPollResult::NoDatagram;
}

BacnetServerPollResult BacnetServer::handleSubscribeCov(
  const BacnetIpEndpoint& source,
  const BacnetSubscribeCovRequestHeader& request) {
  const uint8_t serviceChoice = request.isPropertySubscription ? 0x1CU : 0x05U;
  const BacnetPropertyId property = request.isPropertySubscription
                                      ? request.property
                                      : BacnetPropertyId::PresentValue;
  BacnetValue currentValue;
  if (!readCovValue(request.object, property, request.arrayIndex, currentValue)) {
    uint8_t response[kMaxDatagramSize] = {};
    const size_t size = BacnetProtocol::buildServiceErrorResponse(
      response, sizeof(response), request.invokeId, serviceChoice, 2U, 32U);
    return size != 0U && transport_->send(source, response, size)
             ? BacnetServerPollResult::SubscribeCovErrorSent
             : BacnetServerPollResult::SendFailed;
  }
  if (!request.isPropertySubscription &&
      !readCovValue(request.object, BacnetPropertyId::StatusFlags, kBacnetNoArrayIndex, currentValue)) {
    uint8_t response[kMaxDatagramSize] = {};
    const size_t size = BacnetProtocol::buildServiceErrorResponse(
      response, sizeof(response), request.invokeId, serviceChoice, 2U, 32U);
    return size != 0U && transport_->send(source, response, size)
             ? BacnetServerPollResult::SubscribeCovErrorSent
             : BacnetServerPollResult::SendFailed;
  }
  if (request.hasCovIncrement &&
      (!request.isPropertySubscription || property != BacnetPropertyId::PresentValue ||
       currentValue.type != BacnetValueType::Real)) {
    uint8_t response[kMaxDatagramSize] = {};
    const size_t size = BacnetProtocol::buildServiceErrorResponse(
      response, sizeof(response), request.invokeId, serviceChoice, 2U, 37U);
    return size != 0U && transport_->send(source, response, size)
             ? BacnetServerPollResult::SubscribeCovErrorSent
             : BacnetServerPollResult::SendFailed;
  }

  BacnetServerCovSubscription* matched = nullptr;
  BacnetServerCovSubscription* freeSlot = nullptr;
  for (BacnetServerCovSubscription& subscription : covSubscriptions_) {
    if (subscription.state == BacnetServerCovSubscriptionState::Inactive) {
      if (freeSlot == nullptr) {
        freeSlot = &subscription;
      }
      continue;
    }
    if (endpointEquals(subscription.peer, source) &&
        subscription.processId == request.processId &&
        subscription.object.type == request.object.type &&
        subscription.object.instance == request.object.instance &&
        subscription.isPropertySubscription == request.isPropertySubscription &&
        subscription.property == property && subscription.arrayIndex == request.arrayIndex) {
      matched = &subscription;
      break;
    }
  }

  if (request.lifetimeSeconds == 0U) {
    if (matched != nullptr) {
      const size_t index = static_cast<size_t>(matched - covSubscriptions_);
      *matched = BacnetServerCovSubscription{};
      for (size_t propertyIndex = 0; propertyIndex < kCovObjectPropertyCount; ++propertyIndex) {
        covSnapshotValid_[index][propertyIndex] = false;
      }
      covConfirmedRetryCounts_[index] = 0;
    }
  } else {
    BacnetServerCovSubscription* target = matched != nullptr ? matched : freeSlot;
    if (target == nullptr) {
      uint8_t response[kMaxDatagramSize] = {};
      const size_t size = BacnetProtocol::buildServiceErrorResponse(
        response, sizeof(response), request.invokeId, serviceChoice, 2U, 18U);
      return size != 0U && transport_->send(source, response, size)
               ? BacnetServerPollResult::SubscribeCovErrorSent
               : BacnetServerPollResult::SendFailed;
    }
    const size_t index = static_cast<size_t>(target - covSubscriptions_);
    *target = BacnetServerCovSubscription{};
    target->state = BacnetServerCovSubscriptionState::Active;
    target->peer = source;
    target->processId = request.processId;
    target->object = request.object;
    target->property = property;
    target->arrayIndex = request.arrayIndex;
    target->isPropertySubscription = request.isPropertySubscription;
    target->hasCovIncrement = request.hasCovIncrement;
    target->covIncrement = request.covIncrement;
    target->confirmed = request.issueConfirmedNotifications;
    target->lifetimeSeconds = request.lifetimeSeconds;
    target->expiresAtMs = clock_ == nullptr ? 0U : nowMs() + request.lifetimeSeconds * 1000U;
    for (size_t propertyIndex = 0; propertyIndex < kCovObjectPropertyCount; ++propertyIndex) {
      covSnapshotValid_[index][propertyIndex] = false;
    }
    covConfirmedRetryCounts_[index] = 0;
  }

  uint8_t response[kMaxDatagramSize] = {};
  const size_t size = BacnetProtocol::buildSimpleAckResponse(
    response, sizeof(response), request.invokeId, serviceChoice);
  return size != 0U && transport_->send(source, response, size)
           ? BacnetServerPollResult::SubscribeCovAckSent
           : BacnetServerPollResult::SendFailed;
}

bool BacnetServer::readDeviceProperty(BacnetPropertyId property,
                                      BacnetValue& value) const {
  value = BacnetValue{};
  const auto setText = [&value](const char* text) {
    if (text == nullptr) {
      return false;
    }
    const size_t length = std::strlen(text);
    if (length >= sizeof(value.text)) {
      return false;
    }
    std::memcpy(value.text, text, length + 1);
    value.textLength = length;
    value.type = BacnetValueType::CharacterString;
    return true;
  };
  switch (property) {
    case BacnetPropertyId::ObjectIdentifier:
      value.type = BacnetValueType::ObjectIdentifier;
      value.objectValue = BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::Device),
                                         device_.deviceInstance};
      return true;
    case BacnetPropertyId::ObjectName:
      return setText(device_.objectName);
    case BacnetPropertyId::VendorName:
      return setText(device_.vendorName);
    case BacnetPropertyId::ModelName:
      return setText(device_.modelName);
    case BacnetPropertyId::FirmwareRevision:
      return setText(device_.firmwareRevision);
    case BacnetPropertyId::ApplicationSoftwareVersion:
      return setText(device_.applicationSoftwareVersion != nullptr &&
                         device_.applicationSoftwareVersion[0] != '\0'
                       ? device_.applicationSoftwareVersion
                       : device_.firmwareRevision);
    case BacnetPropertyId::ObjectType:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = static_cast<uint16_t>(BacnetObjectType::Device);
      return true;
    case BacnetPropertyId::SystemStatus:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = 0;
      return true;
    case BacnetPropertyId::VendorIdentifier:
      value.type = BacnetValueType::Unsigned;
      value.unsignedValue = device_.vendorId;
      return true;
    case BacnetPropertyId::ProtocolVersion:
      value.type = BacnetValueType::Unsigned;
      value.unsignedValue = device_.protocolVersion;
      return true;
    case BacnetPropertyId::ProtocolRevision:
      value.type = BacnetValueType::Unsigned;
      value.unsignedValue = device_.protocolRevision;
      return true;
    case BacnetPropertyId::MaxApduLengthAccepted:
      value.type = BacnetValueType::Unsigned;
      value.unsignedValue = device_.maxApduLengthAccepted;
      return true;
    case BacnetPropertyId::SegmentationSupported:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = device_.segmentationSupported;
      return true;
    case BacnetPropertyId::ApduTimeout:
      value.type = BacnetValueType::Unsigned;
      value.unsignedValue = device_.apduTimeout;
      return true;
    case BacnetPropertyId::NumberOfApduRetries:
      value.type = BacnetValueType::Unsigned;
      value.unsignedValue = device_.numberOfApduRetries;
      return true;
    case BacnetPropertyId::DatabaseRevision:
      value.type = BacnetValueType::Unsigned;
      value.unsignedValue = device_.databaseRevision;
      return true;
    case BacnetPropertyId::ProtocolServicesSupported:
      value.type = BacnetValueType::BitString;
      value.bitStringValue = (1UL << 5U) | (1UL << 12U);
      if (binaryOutputCount() != 0)
        value.bitStringValue |= 1UL << 15U;
      value.bitStringBitCount = binaryOutputCount() == 0 ? 13 : 16;
      return true;
    case BacnetPropertyId::ProtocolObjectTypesSupported:
      value.type = BacnetValueType::BitString;
      value.bitStringValue = 1UL << static_cast<uint16_t>(BacnetObjectType::Device);
      if (analogInputCount() != 0) {
        value.bitStringValue |= 1UL << static_cast<uint16_t>(BacnetObjectType::AnalogInput);
      }
      if (analogValueCount_ != 0) {
        value.bitStringValue |= 1UL << static_cast<uint16_t>(BacnetObjectType::AnalogValue);
      }
      if (binaryInputCount() != 0) {
        value.bitStringValue |= 1UL << static_cast<uint16_t>(BacnetObjectType::BinaryInput);
      }
      if (binaryOutputCount() != 0) {
        value.bitStringValue |= 1UL << static_cast<uint16_t>(BacnetObjectType::BinaryOutput);
      }
      value.bitStringBitCount = 9;
      return true;
    default:
      return false;
  }
}

bool BacnetServer::readAnalogValueProperty(
  const BacnetServerAnalogValue& analogValue,
  BacnetPropertyId property,
  BacnetValue& value) const {
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::AnalogValue),
                              analogValue.instance};
  if (const BacnetServerPropertyRegistration* registration =
        findPropertyRegistration(object, property)) {
    return registration->provider(registration->context, value);
  }
  value = BacnetValue{};
  switch (property) {
    case BacnetPropertyId::ObjectIdentifier:
      value.type = BacnetValueType::ObjectIdentifier;
      value.objectValue = BacnetObjectId{
        static_cast<uint16_t>(BacnetObjectType::AnalogValue), analogValue.instance};
      return true;
    case BacnetPropertyId::ObjectName: {
      const size_t length = std::strlen(analogValue.objectName);
      std::memcpy(value.text, analogValue.objectName, length + 1U);
      value.textLength = length;
      value.type = BacnetValueType::CharacterString;
      return true;
    }
    case BacnetPropertyId::ObjectType:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = static_cast<uint16_t>(BacnetObjectType::AnalogValue);
      return true;
    case BacnetPropertyId::PresentValue:
      value.type = BacnetValueType::Real;
      value.realValue = analogValue.presentValueProvider == nullptr
                          ? analogValue.presentValue
                          : analogValue.presentValueProvider(analogValue.presentValueContext);
      return true;
    case BacnetPropertyId::StatusFlags:
      value.type = BacnetValueType::BitString;
      value.bitStringValue = analogValue.outOfService ? 1UL << 3U : 0;
      value.bitStringBitCount = 4;
      return true;
    case BacnetPropertyId::EventState:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = 0;
      return true;
    case BacnetPropertyId::OutOfService:
      value.type = BacnetValueType::Boolean;
      value.booleanValue = analogValue.outOfService;
      return true;
    case BacnetPropertyId::Units:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = analogValue.units;
      return true;
    default:
      return false;
  }
}

bool BacnetServer::readAnalogInputProperty(
  const BacnetServerAnalogInput& analogInput,
  BacnetPropertyId property,
  BacnetValue& value) const {
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::AnalogInput),
                              analogInput.instance};
  if (const BacnetObjectPropertySource* source = findObjectPropertySource(object)) {
    if (source->readOptionalProperty(property, value)) {
      return true;
    }
  }
  if (const BacnetServerPropertyRegistration* registration =
        findPropertyRegistration(object, property)) {
    return registration->provider(registration->context, value);
  }
  value = BacnetValue{};
  switch (property) {
    case BacnetPropertyId::ObjectIdentifier:
      value.type = BacnetValueType::ObjectIdentifier;
      value.objectValue = object;
      return true;
    case BacnetPropertyId::ObjectName: {
      const size_t length = std::strlen(analogInput.objectName);
      std::memcpy(value.text, analogInput.objectName, length + 1U);
      value.textLength = length;
      value.type = BacnetValueType::CharacterString;
      return true;
    }
    case BacnetPropertyId::ObjectType:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = static_cast<uint16_t>(BacnetObjectType::AnalogInput);
      return true;
    case BacnetPropertyId::PresentValue:
      value.type = BacnetValueType::Real;
      value.realValue = analogInput.presentValueProvider == nullptr
                          ? analogInput.presentValue
                          : analogInput.presentValueProvider(analogInput.presentValueContext);
      return true;
    case BacnetPropertyId::StatusFlags:
      value.type = BacnetValueType::BitString;
      value.bitStringValue = analogInput.outOfService ? 1UL << 3U : 0;
      value.bitStringBitCount = 4;
      return true;
    case BacnetPropertyId::EventState:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = 0;
      return true;
    case BacnetPropertyId::OutOfService:
      value.type = BacnetValueType::Boolean;
      value.booleanValue = analogInput.outOfService;
      return true;
    case BacnetPropertyId::Units:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = analogInput.units;
      return true;
    default:
      return false;
  }
}

bool BacnetServer::readBinaryInputProperty(
  const BacnetServerBinaryInput& binaryInput,
  BacnetPropertyId property,
  BacnetValue& value) const {
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::BinaryInput),
                              binaryInput.instance};
  if (const BacnetObjectPropertySource* source = findObjectPropertySource(object)) {
    if (source->readOptionalProperty(property, value)) {
      return true;
    }
  }
  if (const BacnetServerPropertyRegistration* registration =
        findPropertyRegistration(object, property)) {
    return registration->provider(registration->context, value);
  }
  value = BacnetValue{};
  switch (property) {
    case BacnetPropertyId::ObjectIdentifier:
      value.type = BacnetValueType::ObjectIdentifier;
      value.objectValue = object;
      return true;
    case BacnetPropertyId::ObjectName: {
      const size_t length = std::strlen(binaryInput.objectName);
      std::memcpy(value.text, binaryInput.objectName, length + 1U);
      value.textLength = length;
      value.type = BacnetValueType::CharacterString;
      return true;
    }
    case BacnetPropertyId::ObjectType:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = static_cast<uint16_t>(BacnetObjectType::BinaryInput);
      return true;
    case BacnetPropertyId::PresentValue:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = (binaryInput.presentValueProvider == nullptr
                               ? binaryInput.presentValue
                               : binaryInput.presentValueProvider(binaryInput.presentValueContext))
                              ? 1U
                              : 0U;
      return true;
    case BacnetPropertyId::StatusFlags:
      value.type = BacnetValueType::BitString;
      value.bitStringValue = binaryInput.outOfService ? 1UL << 3U : 0;
      value.bitStringBitCount = 4;
      return true;
    case BacnetPropertyId::EventState:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = 0;
      return true;
    case BacnetPropertyId::OutOfService:
      value.type = BacnetValueType::Boolean;
      value.booleanValue = binaryInput.outOfService;
      return true;
    default:
      return false;
  }
}

bool BacnetServer::readBinaryOutputProperty(
  const BacnetServerBinaryOutput& binaryOutput,
  BacnetPropertyId property,
  BacnetValue& value) const {
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::BinaryOutput),
                              binaryOutput.instance};
  if (const BacnetObjectPropertySource* source = findObjectPropertySource(object)) {
    if (source->readOptionalProperty(property, value)) {
      return true;
    }
  }
  if (const BacnetServerPropertyRegistration* registration =
        findPropertyRegistration(object, property)) {
    return registration->provider(registration->context, value);
  }
  value = BacnetValue{};
  switch (property) {
    case BacnetPropertyId::ObjectIdentifier:
      value.type = BacnetValueType::ObjectIdentifier;
      value.objectValue = object;
      return true;
    case BacnetPropertyId::ObjectName: {
      const size_t length = std::strlen(binaryOutput.objectName);
      std::memcpy(value.text, binaryOutput.objectName, length + 1U);
      value.textLength = length;
      value.type = BacnetValueType::CharacterString;
      return true;
    }
    case BacnetPropertyId::ObjectType:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = static_cast<uint16_t>(BacnetObjectType::BinaryOutput);
      return true;
    case BacnetPropertyId::PresentValue:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = binaryOutput.priority.effectiveValue() ? 1U : 0U;
      return true;
    case BacnetPropertyId::StatusFlags:
      value.type = BacnetValueType::BitString;
      value.bitStringValue = binaryOutput.outOfService ? 1UL << 3U : 0U;
      value.bitStringBitCount = 4;
      return true;
    case BacnetPropertyId::EventState:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = 0;
      return true;
    case BacnetPropertyId::OutOfService:
      value.type = BacnetValueType::Boolean;
      value.booleanValue = binaryOutput.outOfService;
      return true;
    case BacnetPropertyId::Reliability:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = binaryOutput.reliability;
      return true;
    case BacnetPropertyId::Polarity:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = binaryOutput.polarity;
      return true;
    case BacnetPropertyId::RelinquishDefault:
      value.type = BacnetValueType::Enumerated;
      value.unsignedValue = binaryOutput.priority.relinquishDefault ? 1U : 0U;
      return true;
    default:
      return false;
  }
}

const BacnetServerPropertyRegistration* BacnetServer::findPropertyRegistration(
  BacnetObjectId object,
  BacnetPropertyId property) const {
  for (size_t index = 0; index < propertyRegistrationCount_; ++index) {
    const BacnetServerPropertyRegistration& registration = propertyRegistrations_[index];
    if (registration.object.type == object.type &&
        registration.object.instance == object.instance &&
        registration.property == property) {
      return &registration;
    }
  }
  return nullptr;
}

size_t BacnetServer::objectPropertyCount(BacnetObjectId object) const {
  size_t count = 0;
  if (object.type == static_cast<uint16_t>(BacnetObjectType::AnalogValue)) {
    count = kAnalogValuePropertyCount;
  } else if (object.type == static_cast<uint16_t>(BacnetObjectType::AnalogInput)) {
    count = kAnalogInputPropertyCount;
  } else if (object.type == static_cast<uint16_t>(BacnetObjectType::BinaryInput)) {
    count = kBinaryInputPropertyCount;
  } else if (object.type == static_cast<uint16_t>(BacnetObjectType::BinaryOutput)) {
    count = kBinaryOutputPropertyCount;
  }
  for (size_t index = 0; index < propertyRegistrationCount_; ++index) {
    const BacnetServerPropertyRegistration& registration = propertyRegistrations_[index];
    if (registration.object.type == object.type &&
        registration.object.instance == object.instance) {
      if (!isBaseProperty(object, registration.property))
        ++count;
    }
  }
  if (const BacnetObjectPropertySource* source = findObjectPropertySource(object)) {
    for (size_t index = 0; index < source->optionalPropertyCount(); ++index) {
      BacnetPropertyId property;
      if (source->optionalPropertyAt(index, property) && !isBaseProperty(object, property)) {
        ++count;
      }
    }
  }
  return count;
}

bool BacnetServer::objectPropertyAt(BacnetObjectId object,
                                    size_t index,
                                    BacnetPropertyId& property) const {
  const BacnetPropertyId* baseProperties = nullptr;
  size_t baseCount = 0;
  if (object.type == static_cast<uint16_t>(BacnetObjectType::AnalogValue)) {
    baseProperties = kAnalogValueProperties;
    baseCount = kAnalogValuePropertyCount;
  } else if (object.type == static_cast<uint16_t>(BacnetObjectType::AnalogInput)) {
    baseProperties = kAnalogInputProperties;
    baseCount = kAnalogInputPropertyCount;
  } else if (object.type == static_cast<uint16_t>(BacnetObjectType::BinaryInput)) {
    baseProperties = kBinaryInputProperties;
    baseCount = kBinaryInputPropertyCount;
  } else if (object.type == static_cast<uint16_t>(BacnetObjectType::BinaryOutput)) {
    baseProperties = kBinaryOutputProperties;
    baseCount = kBinaryOutputPropertyCount;
  } else {
    return false;
  }
  if (index < baseCount) {
    property = baseProperties[index];
    return true;
  }
  size_t optionalIndex = index - baseCount;
  for (size_t registrationIndex = 0;
       registrationIndex < propertyRegistrationCount_;
       ++registrationIndex) {
    const BacnetServerPropertyRegistration& registration =
      propertyRegistrations_[registrationIndex];
    if (registration.object.type != object.type ||
        registration.object.instance != object.instance) {
      continue;
    }
    bool baseProperty = false;
    for (size_t baseIndex = 0; baseIndex < baseCount; ++baseIndex) {
      if (baseProperties[baseIndex] == registration.property) {
        baseProperty = true;
        break;
      }
    }
    if (baseProperty)
      continue;
    if (optionalIndex == 0) {
      property = registration.property;
      return true;
    }
    --optionalIndex;
  }
  if (const BacnetObjectPropertySource* source = findObjectPropertySource(object)) {
    for (size_t sourceIndex = 0; sourceIndex < source->optionalPropertyCount(); ++sourceIndex) {
      BacnetPropertyId sourceProperty;
      if (!source->optionalPropertyAt(sourceIndex, sourceProperty) ||
          isBaseProperty(object, sourceProperty)) {
        continue;
      }
      if (optionalIndex == 0) {
        property = sourceProperty;
        return true;
      }
      --optionalIndex;
    }
  }
  return false;
}

bool BacnetServer::objectPropertyEntry(const void* context,
                                       size_t index,
                                       BacnetPropertyId& property) {
  const auto* listContext = static_cast<const ObjectPropertyListContext*>(context);
  return listContext != nullptr && listContext->server != nullptr &&
         listContext->server->objectPropertyAt(listContext->object, index, property);
}

bool BacnetServer::binaryOutputPriorityEntry(const void* context,
                                             size_t index,
                                             BacnetValue& value) {
  const auto* output = static_cast<const BacnetServerBinaryOutput*>(context);
  if (output == nullptr || index >= BacnetCommandPriority<bool>::kSlotCount) {
    return false;
  }
  value = BacnetValue{};
  value.type = output->priority.occupied[index] ? BacnetValueType::Enumerated
                                                : BacnetValueType::Null;
  value.unsignedValue = output->priority.slots[index] ? 1U : 0U;
  return true;
}

const BacnetServerAnalogValue* BacnetServer::findAnalogValue(uint32_t instance) const {
  for (size_t index = 0; index < analogValueCount_; ++index) {
    if (analogValues_[index].instance == instance) {
      return &analogValues_[index];
    }
  }
  return nullptr;
}

const BacnetServerAnalogInput* BacnetServer::findAnalogInput(uint32_t instance) const {
  for (size_t index = 0; index < analogInputCount(); ++index) {
    const BacnetServerAnalogInput* input = analogInputAt(index);
    if (input != nullptr && input->instance == instance)
      return input;
  }
  return nullptr;
}

const BacnetServerBinaryInput* BacnetServer::findBinaryInput(uint32_t instance) const {
  for (size_t index = 0; index < binaryInputCount(); ++index) {
    const BacnetServerBinaryInput* input = binaryInputAt(index);
    if (input != nullptr && input->instance == instance)
      return input;
  }
  return nullptr;
}

BacnetServerBinaryOutput* BacnetServer::findBinaryOutput(uint32_t instance) const {
  for (size_t index = 0; index < binaryOutputCount(); ++index) {
    BacnetServerBinaryOutput* output = binaryOutputAt(index);
    if (output != nullptr && output->instance == instance)
      return output;
  }
  return nullptr;
}

const BacnetServerAnalogInput* BacnetServer::analogInputAt(size_t index) const {
  return objectCentricAnalogInputCount_ != 0
           ? (index < objectCentricAnalogInputCount_ ? objectCentricAnalogInputs_[index] : nullptr)
           : (index < analogInputCount_ ? &analogInputs_[index] : nullptr);
}

const BacnetServerBinaryInput* BacnetServer::binaryInputAt(size_t index) const {
  return objectCentricBinaryInputCount_ != 0
           ? (index < objectCentricBinaryInputCount_ ? objectCentricBinaryInputs_[index] : nullptr)
           : (index < binaryInputCount_ ? &binaryInputs_[index] : nullptr);
}

BacnetServerBinaryOutput* BacnetServer::binaryOutputAt(size_t index) const {
  return objectCentricBinaryOutputCount_ != 0
           ? (index < objectCentricBinaryOutputCount_ ? objectCentricBinaryOutputs_[index] : nullptr)
           : (index < binaryOutputCount_ ? &binaryOutputs_[index] : nullptr);
}

const BacnetObjectPropertySource* BacnetServer::findObjectPropertySource(
  BacnetObjectId object) const {
  const BacnetObjectPropertySource* const* sources = nullptr;
  size_t count = 0;
  if (object.type == static_cast<uint16_t>(BacnetObjectType::AnalogInput)) {
    sources = objectCentricAnalogInputProperties_;
    count = objectCentricAnalogInputCount_;
  } else if (object.type == static_cast<uint16_t>(BacnetObjectType::BinaryInput)) {
    sources = objectCentricBinaryInputProperties_;
    count = objectCentricBinaryInputCount_;
  } else if (object.type == static_cast<uint16_t>(BacnetObjectType::BinaryOutput)) {
    sources = objectCentricBinaryOutputProperties_;
    count = objectCentricBinaryOutputCount_;
  }
  for (size_t index = 0; index < count; ++index) {
    if (sources[index] != nullptr && sources[index]->objectId().type == object.type &&
        sources[index]->objectId().instance == object.instance) {
      return sources[index];
    }
  }
  return nullptr;
}

bool BacnetServer::objectListEntry(const void* context,
                                   size_t index,
                                   BacnetObjectId& object) {
  const auto* server = static_cast<const BacnetServer*>(context);
  const size_t objectCount = server == nullptr ? 0 : server->analogValueCount_ + server->analogInputCount() + server->binaryInputCount() + server->binaryOutputCount() + 1U;
  if (server == nullptr || index >= objectCount) {
    return false;
  }
  if (index == 0) {
    object = BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::Device),
                            server->device_.deviceInstance};
    return true;
  }
  --index;
  if (index < server->analogValueCount_) {
    object = BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::AnalogValue),
                            server->analogValues_[index].instance};
    return true;
  }
  index -= server->analogValueCount_;
  if (index < server->analogInputCount()) {
    const BacnetServerAnalogInput* input = server->analogInputAt(index);
    if (input == nullptr) {
      return false;
    }
    object = BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::AnalogInput),
                            input->instance};
    return true;
  }
  index -= server->analogInputCount();
  if (index < server->binaryInputCount()) {
    const BacnetServerBinaryInput* input = server->binaryInputAt(index);
    if (input == nullptr) {
      return false;
    }
    object = BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::BinaryInput),
                            input->instance};
    return true;
  }
  index -= server->binaryInputCount();
  const BacnetServerBinaryOutput* output = server->binaryOutputAt(index);
  if (output == nullptr) {
    return false;
  }
  object = BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::BinaryOutput),
                          output->instance};
  return true;
}

bool BacnetServer::sendIAm(const BacnetIpEndpoint& destination) {
  uint8_t response[BacnetProtocol::kMaxIAmResponseSize] = {};
  const BacnetIAmDeviceInfo deviceInfo{
    device_.deviceInstance,
    device_.maxApduLengthAccepted,
    device_.segmentationSupported,
    device_.vendorId,
  };
  const size_t responseSize =
    BacnetProtocol::buildIAmResponse(response, sizeof(response), deviceInfo);
  return responseSize != 0 && transport_->send(destination, response, responseSize);
}

bool BacnetServer::sendReject(const BacnetIpEndpoint& destination,
                              uint8_t invokeId) {
  uint8_t response[BacnetProtocol::kRejectResponseSize] = {};
  const size_t responseSize = BacnetProtocol::buildRejectResponse(
    response, sizeof(response), invokeId, kRejectReasonUnrecognizedService);
  return responseSize != 0 && transport_->send(destination, response, responseSize);
}
