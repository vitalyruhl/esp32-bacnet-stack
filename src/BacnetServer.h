// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <cstddef>
#include <cstdint>

#include "portable/BacnetProtocol.h"
#include "portable/BacnetRuntime.h"
#include "portable/BacnetCommandPriority.h"

struct BacnetServerDevice {
  // Required BACnet Device identity and profile values.
  uint32_t deviceInstance = 0;
  // Supplied by the final device provider; no production ID is assigned here.
  uint16_t vendorId = 0;
  const char* objectName = nullptr;
  const char* vendorName = nullptr;
  const char* modelName = nullptr;
  const char* firmwareRevision = nullptr;
  // Optional. A null or empty value exposes firmwareRevision instead.
  const char* applicationSoftwareVersion = nullptr;
  uint32_t maxApduLengthAccepted = 1476;
  uint32_t apduTimeout = 3000;
  uint32_t numberOfApduRetries = 3;
  uint32_t databaseRevision = 0;
  uint8_t protocolVersion = 1;
  uint8_t protocolRevision = 14;
  uint8_t segmentationSupported = 3;
};

using BacnetServerAnalogValueProvider = float (*)(void* context);
using BacnetServerBinaryInputProvider = bool (*)(void* context);
using BacnetServerBinaryOutputApply = void (*)(void* context,
                                               bool presentValue,
                                               bool outOfService);
using BacnetServerPropertyProvider = bool (*)(const void* context, BacnetValue& value);

// Caller-owned optional property descriptor. Register only properties that an
// object actually supports; the server neither owns nor allocates descriptors
// or their contexts. An empty string is a valid Description value, while a
// null description context must simply not be registered.
struct BacnetServerPropertyRegistration {
  BacnetObjectId object;
  BacnetPropertyId property = BacnetPropertyId::ObjectName;
  BacnetServerPropertyProvider provider = nullptr;
  const void* context = nullptr;
};

// Caller-owned Analog Value configuration. The server borrows this array and
// does not allocate an object database. A provider is polled only while
// encoding Present_Value; a null provider exposes the stored presentValue.
struct BacnetServerAnalogValue {
  uint32_t instance = 0;
  const char* objectName = nullptr;
  float presentValue = 0.0F;
  uint32_t units = 0;
  bool outOfService = false;
  BacnetServerAnalogValueProvider presentValueProvider = nullptr;
  void* presentValueContext = nullptr;
};

// Caller-owned Analog Input configuration. This has the same read-only value
// provider contract as Analog Value, while advertising the correct BACnet
// process-object type.
struct BacnetServerAnalogInput {
  uint32_t instance = 0;
  const char* objectName = nullptr;
  float presentValue = 0.0F;
  uint32_t units = 0;
  bool outOfService = false;
  BacnetServerAnalogValueProvider presentValueProvider = nullptr;
  void* presentValueContext = nullptr;
};

// Caller-owned Binary Input configuration. Present_Value is encoded as the
// BACnet binary enumeration (inactive=0, active=1), not as a Boolean.
struct BacnetServerBinaryInput {
  uint32_t instance = 0;
  const char* objectName = nullptr;
  bool presentValue = false;
  bool outOfService = false;
  BacnetServerBinaryInputProvider presentValueProvider = nullptr;
  void* presentValueContext = nullptr;
};

// Caller-owned commandable Binary Output. The priority state is portable and
// retained by the application-owned object; a physical binding is optional.
struct BacnetServerBinaryOutput {
  uint32_t instance = 0;
  const char* objectName = nullptr;
  BacnetCommandPriority<bool> priority;
  bool outOfService = false;
  uint32_t polarity = 0;
  uint32_t reliability = 0;
  BacnetServerBinaryOutputApply apply = nullptr;
  void* applyContext = nullptr;
  // Local application writes may opt into a per-object priority. Incoming
  // BACnet WriteProperty requests never consult these fields.
  uint8_t localWritePriority = 16;
  bool hasLocalWritePriority = false;
};

// Result returned by the object-centric configuration API. The API never
// allocates; callers keep configured objects and bound values alive while the
// server is using them.
enum class BacnetObjectConfigurationStatus : uint8_t {
  Ok,
  InvalidArgument,
  PropertyTypeMismatch,
  PropertyNotSupported,
  DuplicateProperty,
  PropertyCapacityExceeded,
  ServerCapacityExceeded,
  RegistrationModeConflict,
};

// Allocation-free context for the first invalid high-level object operation.
// The object owns the referenced name; no error text is copied or allocated.
struct BacnetObjectConfigurationError {
  BacnetObjectConfigurationStatus status = BacnetObjectConfigurationStatus::Ok;
  BacnetObjectId object;
  BacnetPropertyId property = BacnetPropertyId::ObjectIdentifier;
  const char* objectName = nullptr;
};

const char* bacnetObjectConfigurationStatusText(BacnetObjectConfigurationStatus status);
const char* bacnetPropertyIdText(BacnetPropertyId property);

// Explicit wrappers keep enumerated and bit-string optional properties
// unambiguous at the call site.
struct BacnetEnumeratedValue {
  uint32_t value = 0;
};

struct BacnetServerStatusFlags {
  uint32_t value = 0;
  uint8_t bitCount = 4;
};

enum class BacnetObjectPropertyValueType : uint8_t {
  CharacterString,
  Real,
  RealReference,
  Boolean,
  BooleanReference,
  Enumerated,
  EnumeratedReference,
  BitString,
  BitStringReference,
};

struct BacnetObjectPropertySlot {
  BacnetPropertyId property = BacnetPropertyId::ObjectName;
  BacnetObjectPropertyValueType type = BacnetObjectPropertyValueType::CharacterString;
  union Value {
    const char* text;
    float realValue;
    const float* realReference;
    bool booleanValue;
    const bool* booleanReference;
    BacnetEnumeratedValue enumeratedValue;
    const BacnetEnumeratedValue* enumeratedReference;
    BacnetServerStatusFlags bitStringValue;
    const BacnetServerStatusFlags* bitStringReference;

    constexpr Value() : text(nullptr) {}
  } value;
};

class BacnetObjectPropertySource {
public:
  virtual ~BacnetObjectPropertySource() = default;
  virtual BacnetObjectId objectId() const = 0;
  virtual size_t optionalPropertyCount() const = 0;
  virtual bool optionalPropertyAt(size_t index, BacnetPropertyId& property) const = 0;
  virtual bool readOptionalProperty(BacnetPropertyId property,
                                    BacnetValue& value) const = 0;
};

class BacnetObjectWithProperties : public BacnetObjectPropertySource {
public:
  BacnetObjectConfigurationStatus configurationStatus() const;
  bool isConfigurationValid() const;

protected:
  void initializeProperties(BacnetObjectPropertySlot* slots, size_t capacity);
  BacnetObjectConfigurationStatus addPropertyValue(
    BacnetPropertyId property,
    BacnetObjectPropertyValueType type,
    BacnetObjectPropertySlot::Value value);
  BacnetObjectConfigurationStatus recordConfigurationStatus(
    BacnetObjectConfigurationStatus status,
    BacnetPropertyId property);
  BacnetObjectConfigurationError configurationErrorFor(BacnetObjectId object,
                                                       const char* objectName) const;
  bool readStoredProperty(BacnetPropertyId property, BacnetValue& value) const;
  bool storedPropertyAt(size_t index, BacnetPropertyId& property) const;
  size_t storedPropertyCount() const;

  virtual bool supportsProperty(BacnetPropertyId property,
                                BacnetObjectPropertyValueType type) const = 0;

private:
  BacnetObjectPropertySlot* propertySlots_ = nullptr;
  size_t propertyCapacity_ = 0;
  size_t propertyCount_ = 0;
  BacnetObjectConfigurationStatus configurationStatus_ = BacnetObjectConfigurationStatus::Ok;
  BacnetPropertyId configurationErrorProperty_ = BacnetPropertyId::ObjectIdentifier;
};

// The object-centric classes are thin, allocation-free facades over the
// established caller-owned server structures. The low-level array API remains
// available for advanced and generated use cases.
class BacnetAnalogInput final : public BacnetServerAnalogInput,
                                public BacnetObjectWithProperties {
public:
  static constexpr size_t kMaxOptionalProperties = 7;

  BacnetAnalogInput();
  BacnetObjectConfigurationStatus configure(uint32_t instance, const char* objectName);
  BacnetObjectConfigurationStatus bindPresentValue(const float* value);
  void setUnits(uint32_t engineeringUnits);
  BacnetObjectConfigurationStatus addProperty(BacnetPropertyId property, const char* value);
  BacnetObjectConfigurationStatus addProperty(BacnetPropertyId property, float value);
  BacnetObjectConfigurationStatus addProperty(BacnetPropertyId property, const float* value);
  BacnetObjectConfigurationStatus addProperty(BacnetPropertyId property,
                                              BacnetEnumeratedValue value);
  BacnetObjectConfigurationStatus addProperty(BacnetPropertyId property,
                                              const BacnetEnumeratedValue* value);
  BacnetObjectConfigurationStatus addProperty(BacnetPropertyId property,
                                              BacnetServerStatusFlags value);
  BacnetObjectConfigurationStatus addProperty(BacnetPropertyId property,
                                              const BacnetServerStatusFlags* value);
  BacnetObjectConfigurationError configurationError() const;
  BacnetObjectId objectId() const override;
  size_t optionalPropertyCount() const override;
  bool optionalPropertyAt(size_t index, BacnetPropertyId& property) const override;
  bool readOptionalProperty(BacnetPropertyId property, BacnetValue& value) const override;

private:
  bool supportsProperty(BacnetPropertyId property,
                        BacnetObjectPropertyValueType type) const override;
  BacnetObjectPropertySlot properties_[kMaxOptionalProperties] = {};
};

class BacnetBinaryInput final : public BacnetServerBinaryInput,
                                public BacnetObjectWithProperties {
public:
  static constexpr size_t kMaxOptionalProperties = 4;

  BacnetBinaryInput();
  BacnetObjectConfigurationStatus configure(uint32_t instance, const char* objectName);
  BacnetObjectConfigurationStatus bindPresentValue(const bool* value);
  BacnetObjectConfigurationStatus addProperty(BacnetPropertyId property, const char* value);
  BacnetObjectConfigurationStatus addProperty(BacnetPropertyId property,
                                              BacnetEnumeratedValue value);
  BacnetObjectConfigurationStatus addProperty(BacnetPropertyId property,
                                              const BacnetEnumeratedValue* value);
  BacnetObjectConfigurationStatus addProperty(BacnetPropertyId property,
                                              BacnetServerStatusFlags value);
  BacnetObjectConfigurationStatus addProperty(BacnetPropertyId property,
                                              const BacnetServerStatusFlags* value);
  BacnetObjectConfigurationError configurationError() const;
  BacnetObjectId objectId() const override;
  size_t optionalPropertyCount() const override;
  bool optionalPropertyAt(size_t index, BacnetPropertyId& property) const override;
  bool readOptionalProperty(BacnetPropertyId property, BacnetValue& value) const override;

private:
  bool supportsProperty(BacnetPropertyId property,
                        BacnetObjectPropertyValueType type) const override;
  BacnetObjectPropertySlot properties_[kMaxOptionalProperties] = {};
};

class BacnetBinaryOutput final : public BacnetServerBinaryOutput,
                                 public BacnetObjectWithProperties {
public:
  static constexpr size_t kMaxOptionalProperties = 1;

  BacnetBinaryOutput();
  BacnetObjectConfigurationStatus configure(uint32_t instance, const char* objectName);
  void setRelinquishDefault(bool value);
  // Local application default only. Network WriteProperty priorities remain
  // governed by BACnet: requested 1..16, or 16 when omitted.
  bool setLocalWritePriority(uint8_t value);
  bool writeValue(bool value);
  // Priority zero updates Relinquish_Default; 1..16 update Priority_Array.
  bool writeValue(bool value, uint8_t priority);
  bool relinquish(uint8_t priority);
  void attachOutput(BacnetServerBinaryOutputApply apply, void* context);
  BacnetObjectConfigurationStatus addProperty(BacnetPropertyId property, const char* value);
  BacnetObjectConfigurationError configurationError() const;
  BacnetObjectId objectId() const override;
  size_t optionalPropertyCount() const override;
  bool optionalPropertyAt(size_t index, BacnetPropertyId& property) const override;
  bool readOptionalProperty(BacnetPropertyId property, BacnetValue& value) const override;

private:
  friend class BacnetServer;
  void bindServerLocalWritePriority(const uint8_t* priority);
  bool applyLocalWrite(bool value, uint8_t priority, bool relinquish);
  bool supportsProperty(BacnetPropertyId property,
                        BacnetObjectPropertyValueType type) const override;
  BacnetObjectPropertySlot properties_[kMaxOptionalProperties] = {};
  const uint8_t* serverLocalWritePriority_ = nullptr;
};

enum class BacnetServerPollResult : uint8_t {
  NotRunning,
  NoDatagram,
  Ignored,
  Malformed,
  IAmSent,
  RejectSent,
  ReadPropertyAckSent,
  ReadPropertyErrorSent,
  WritePropertyAckSent,
  WritePropertyErrorSent,
  SubscribeCovAckSent,
  SubscribeCovErrorSent,
  CovNotificationSent,
  SendFailed,
};

enum class BacnetServerCovSubscriptionState : uint8_t {
  Inactive,
  Active,
  AwaitingConfirmedAck,
};

// Read-only subscription diagnostics. The server keeps the backing table
// fixed-size and caller-independent; this view never exposes packet storage.
struct BacnetServerCovSubscription {
  BacnetServerCovSubscriptionState state = BacnetServerCovSubscriptionState::Inactive;
  BacnetIpEndpoint peer;
  uint32_t processId = 0;
  BacnetObjectId object;
  BacnetPropertyId property = BacnetPropertyId::PresentValue;
  uint32_t arrayIndex = kBacnetNoArrayIndex;
  bool isPropertySubscription = false;
  bool hasCovIncrement = false;
  float covIncrement = 0.0F;
  bool confirmed = false;
  uint32_t lifetimeSeconds = 0;
  uint32_t expiresAtMs = 0;
  uint32_t lastSentMs = 0;
  uint32_t lastAckMs = 0;
  uint8_t pendingInvokeId = 0;
};

// Optional, synchronous COV-path observation for local diagnostics. The
// listener borrows the subscription view and must not retain it or block.
enum class BacnetServerCovDiagnosticEvent : uint8_t {
  SubscriptionActivated,
  ChangeDetected,
  NotificationSent,
  NotificationSendFailed,
};

struct BacnetServerCovDiagnostic {
  BacnetServerCovDiagnosticEvent event = BacnetServerCovDiagnosticEvent::SubscriptionActivated;
  BacnetServerCovSubscription subscription;
};

using BacnetServerCovDiagnosticListener = void (*)(
  void* context,
  const BacnetServerCovDiagnostic& diagnostic);

enum class BacnetServerActivityService : uint8_t {
  ReadProperty,
  WriteProperty,
};

// Optional request-observation hook for platform adapters and local diagnostics.
// It exposes portable BACnet endpoint and request metadata only; it never owns
// data, logs, allocates, or changes BACnet behavior.
struct BacnetServerActivity {
  BacnetIpEndpoint peer;
  BacnetServerActivityService service = BacnetServerActivityService::ReadProperty;
  BacnetObjectId object;
  BacnetPropertyId property = BacnetPropertyId::ObjectName;
  bool hasPriority = false;
  uint8_t priority = 0;
};

using BacnetServerActivityListener = void (*)(void* context,
                                              const BacnetServerActivity& activity);

class BacnetServer {
public:
  static constexpr uint16_t kDefaultPort = 47808;
  static constexpr uint8_t kRejectReasonUnrecognizedService = 9;
  static constexpr size_t kMaxDatagramSize = 1476;
  static constexpr size_t kMaxCovSubscriptions = 8;
  static constexpr size_t kCovObjectPropertyCount = 2;

  BacnetServer() = default;
  explicit BacnetServer(BacnetDatagramTransport& transport);
  BacnetServer(const BacnetServer&) = delete;
  BacnetServer& operator=(const BacnetServer&) = delete;
  BacnetServer(BacnetServer&&) = delete;
  BacnetServer& operator=(BacnetServer&&) = delete;

  // The server borrows its transport; it must outlive the server and must not
  // be shared between concurrently running server instances.
  // This stateless MVP does not require a clock or logger yet.
  bool setTransport(BacnetDatagramTransport& transport);
  // Binds the borrowed transport to localPort. Omitting it uses BACnet/IP UDP
  // port 47808; this is the sole local-port configuration for the server.
  bool begin(const BacnetServerDevice& configuredDevice,
             uint16_t localPort = kDefaultPort);
  bool begin(uint32_t deviceInstanceValue, uint16_t localPort = kDefaultPort);
  void end();

  // The caller keeps the array and all referenced strings alive while the
  // server is running. Passing nullptr with zero entries unregisters all AVs.
  bool setAnalogValues(BacnetServerAnalogValue* analogValues,
                       size_t count);
  size_t analogValueCount() const;
  bool setAnalogInputs(BacnetServerAnalogInput* analogInputs,
                       size_t count);
  size_t analogInputCount() const;
  bool setBinaryInputs(BacnetServerBinaryInput* binaryInputs,
                       size_t count);
  size_t binaryInputCount() const;
  bool setBinaryOutputs(BacnetServerBinaryOutput* binaryOutputs,
                        size_t count);
  size_t binaryOutputCount() const;
  static constexpr size_t kMaxObjectCentricAnalogInputs = 8;
  static constexpr size_t kMaxObjectCentricBinaryInputs = 8;
  static constexpr size_t kMaxObjectCentricBinaryOutputs = 8;
  BacnetObjectConfigurationStatus addObject(BacnetAnalogInput& object);
  BacnetObjectConfigurationStatus addObject(BacnetBinaryInput& object);
  BacnetObjectConfigurationStatus addObject(BacnetBinaryOutput& object);
  // Optional caller-owned ReadProperty descriptors. Registrations extend an
  // object's Property_List only when present and must remain valid while the
  // server is running. Passing nullptr with zero entries unregisters all.
  bool setPropertyRegistrations(
    const BacnetServerPropertyRegistration* registrations,
    size_t count);
  size_t propertyRegistrationCount() const;

  bool isRunning() const;
  const BacnetServerDevice& device() const;
  uint32_t deviceInstance() const;
  uint16_t port() const;
  // Applies only to local application writes issued through commandable
  // object facades. Incoming BACnet WriteProperty requests are unaffected.
  bool setLocalWritePriority(uint8_t priority);
  uint8_t localWritePriority() const;
  void setClock(const BacnetMonotonicClock* clock);
  size_t covSubscriptionCount() const;
  bool covSubscriptionAt(size_t index, BacnetServerCovSubscription& subscription) const;
  // Observes accepted ReadProperty and WriteProperty requests. The listener is
  // invoked synchronously during poll(), so it must be short and non-blocking.
  void setActivityListener(BacnetServerActivityListener listener, void* context = nullptr);
  // Observes COV subscription activation, detected changes, and notification
  // send outcomes. It is intended for concise local diagnostics only.
  void setCovDiagnosticListener(BacnetServerCovDiagnosticListener listener,
                                void* context = nullptr);
  BacnetServerPollResult poll();

private:
  bool sendIAm(const BacnetIpEndpoint& destination);
  bool sendReject(const BacnetIpEndpoint& destination, uint8_t invokeId);
  BacnetServerPollResult handleReadProperty(
    const BacnetIpEndpoint& source,
    const BacnetReadPropertyRequestHeader& request);
  BacnetServerPollResult handleWriteProperty(
    const BacnetIpEndpoint& source,
    const BacnetWritePropertyRequestHeader& request);
  BacnetServerPollResult handleSubscribeCov(
    const BacnetIpEndpoint& source,
    const BacnetSubscribeCovRequestHeader& request);
  BacnetServerPollResult processCovSubscriptions();
  bool readCovValue(BacnetObjectId object,
                    BacnetPropertyId property,
                    uint32_t arrayIndex,
                    BacnetValue& value) const;
  bool collectCovValues(const BacnetServerCovSubscription& subscription,
                        BacnetCovPropertyValue* values,
                        size_t capacity,
                        size_t& count) const;
  bool covValuesChanged(const BacnetServerCovSubscription& subscription,
                        const BacnetCovPropertyValue* values,
                        size_t count) const;
  void storeCovValues(BacnetServerCovSubscription& subscription,
                      const BacnetCovPropertyValue* values,
                      size_t count);
  bool sendCovNotification(BacnetServerCovSubscription& subscription,
                           const BacnetCovPropertyValue* values,
                           size_t valueCount,
                           uint32_t nowMs);
  void emitCovDiagnostic(BacnetServerCovDiagnosticEvent event,
                         const BacnetServerCovSubscription& subscription) const;
  static bool endpointEquals(const BacnetIpEndpoint& left,
                             const BacnetIpEndpoint& right);
  uint32_t nowMs() const;
  bool readDeviceProperty(BacnetPropertyId property, BacnetValue& value) const;
  bool readAnalogValueProperty(const BacnetServerAnalogValue& analogValue,
                               BacnetPropertyId property,
                               BacnetValue& value) const;
  bool readAnalogInputProperty(const BacnetServerAnalogInput& analogInput,
                               BacnetPropertyId property,
                               BacnetValue& value) const;
  bool readBinaryInputProperty(const BacnetServerBinaryInput& binaryInput,
                               BacnetPropertyId property,
                               BacnetValue& value) const;
  bool readBinaryOutputProperty(const BacnetServerBinaryOutput& binaryOutput,
                                BacnetPropertyId property,
                                BacnetValue& value) const;
  const BacnetServerAnalogValue* findAnalogValue(uint32_t instance) const;
  const BacnetServerAnalogInput* findAnalogInput(uint32_t instance) const;
  const BacnetServerBinaryInput* findBinaryInput(uint32_t instance) const;
  BacnetServerBinaryOutput* findBinaryOutput(uint32_t instance) const;
  const BacnetObjectPropertySource* findObjectPropertySource(
    BacnetObjectId object) const;
  const BacnetServerAnalogInput* analogInputAt(size_t index) const;
  const BacnetServerBinaryInput* binaryInputAt(size_t index) const;
  BacnetServerBinaryOutput* binaryOutputAt(size_t index) const;
  const BacnetServerPropertyRegistration* findPropertyRegistration(
    BacnetObjectId object,
    BacnetPropertyId property) const;
  size_t objectPropertyCount(BacnetObjectId object) const;
  bool objectPropertyAt(BacnetObjectId object,
                        size_t index,
                        BacnetPropertyId& property) const;
  static bool objectListEntry(const void* context,
                              size_t index,
                              BacnetObjectId& object);
  static bool objectPropertyEntry(const void* context,
                                  size_t index,
                                  BacnetPropertyId& property);
  static bool binaryOutputPriorityEntry(const void* context,
                                        size_t index,
                                        BacnetValue& value);

  BacnetDatagramTransport* transport_ = nullptr; // Non-owning.
  bool running_ = false;
  BacnetServerDevice device_;
  uint16_t port_ = kDefaultPort;
  BacnetServerAnalogValue* analogValues_ = nullptr; // Caller-owned.
  size_t analogValueCount_ = 0;
  BacnetServerAnalogInput* analogInputs_ = nullptr; // Caller-owned.
  size_t analogInputCount_ = 0;
  BacnetServerAnalogInput* objectCentricAnalogInputs_[kMaxObjectCentricAnalogInputs] = {};
  BacnetObjectPropertySource* objectCentricAnalogInputProperties_[kMaxObjectCentricAnalogInputs] = {};
  size_t objectCentricAnalogInputCount_ = 0;
  BacnetServerBinaryInput* binaryInputs_ = nullptr; // Caller-owned.
  size_t binaryInputCount_ = 0;
  BacnetServerBinaryInput* objectCentricBinaryInputs_[kMaxObjectCentricBinaryInputs] = {};
  BacnetObjectPropertySource* objectCentricBinaryInputProperties_[kMaxObjectCentricBinaryInputs] = {};
  size_t objectCentricBinaryInputCount_ = 0;
  const BacnetServerPropertyRegistration* propertyRegistrations_ = nullptr;
  size_t propertyRegistrationCount_ = 0;
  BacnetServerBinaryOutput* binaryOutputs_ = nullptr; // Caller-owned.
  size_t binaryOutputCount_ = 0;
  BacnetServerBinaryOutput* objectCentricBinaryOutputs_[kMaxObjectCentricBinaryOutputs] = {};
  BacnetObjectPropertySource* objectCentricBinaryOutputProperties_[kMaxObjectCentricBinaryOutputs] = {};
  size_t objectCentricBinaryOutputCount_ = 0;
  BacnetServerActivityListener activityListener_ = nullptr;
  void* activityListenerContext_ = nullptr;
  BacnetServerCovDiagnosticListener covDiagnosticListener_ = nullptr;
  void* covDiagnosticListenerContext_ = nullptr;
  uint8_t localWritePriority_ = 16;
  const BacnetMonotonicClock* clock_ = nullptr; // Non-owning.
  BacnetServerCovSubscription covSubscriptions_[kMaxCovSubscriptions] = {};
  BacnetValueType covSnapshotTypes_[kMaxCovSubscriptions][kCovObjectPropertyCount] = {};
  bool covSnapshotBoolean_[kMaxCovSubscriptions][kCovObjectPropertyCount] = {};
  uint32_t covSnapshotUnsigned_[kMaxCovSubscriptions][kCovObjectPropertyCount] = {};
  float covSnapshotReal_[kMaxCovSubscriptions][kCovObjectPropertyCount] = {};
  uint32_t covSnapshotBitString_[kMaxCovSubscriptions][kCovObjectPropertyCount] = {};
  uint8_t covSnapshotBitCount_[kMaxCovSubscriptions][kCovObjectPropertyCount] = {};
  bool covSnapshotValid_[kMaxCovSubscriptions][kCovObjectPropertyCount] = {};
  uint8_t covConfirmedRetryCounts_[kMaxCovSubscriptions] = {};
  uint8_t covSendRetryCounts_[kMaxCovSubscriptions] = {};
  uint32_t covRetryAtMs_[kMaxCovSubscriptions] = {};
  uint8_t nextCovInvokeId_ = 1;
};
