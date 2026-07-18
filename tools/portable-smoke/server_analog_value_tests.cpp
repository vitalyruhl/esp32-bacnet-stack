// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetServer.h"
#include "portable/BacnetAnalogValueLimits.h"

#include <cstring>
#include <type_traits>

namespace {

class TestTransport final : public BacnetDatagramTransport {
public:
  bool begin(uint16_t localPort) override {
    lastBeginPort = localPort;
    ++beginCalls;
    return true;
  }

  void end() override {}

  bool send(const BacnetIpEndpoint& destination,
            const uint8_t* data,
            size_t length) override {
    if (data == nullptr || length > sizeof(lastSent)) {
      return false;
    }
    lastDestination = destination;
    std::memcpy(lastSent, data, length);
    lastSentLength = length;
    ++sendCalls;
    return true;
  }

  size_t receive(uint8_t* buffer,
                 size_t capacity,
                 BacnetIpEndpoint& source) override {
    if (incomingLength == 0 || incomingLength > capacity) {
      return 0;
    }
    std::memcpy(buffer, incoming, incomingLength);
    source = incomingSource;
    const size_t result = incomingLength;
    incomingLength = 0;
    return result;
  }

  void queue(const uint8_t* data,
             size_t length,
             const BacnetIpEndpoint& source) {
    if (data == nullptr || length > sizeof(incoming)) {
      incomingLength = 0;
      return;
    }
    std::memcpy(incoming, data, length);
    incomingLength = length;
    incomingSource = source;
  }

  void idle() override {}

  uint8_t incoming[BacnetServer::kMaxDatagramSize] = {};
  size_t incomingLength = 0;
  BacnetIpEndpoint incomingSource;
  uint8_t lastSent[BacnetServer::kMaxDatagramSize] = {};
  size_t lastSentLength = 0;
  BacnetIpEndpoint lastDestination;
  uint32_t sendCalls = 0;
  uint16_t lastBeginPort = 0;
  uint32_t beginCalls = 0;
};

struct ProviderState {
  float value = 0.0F;
  uint32_t calls = 0;
};

float readProviderValue(void* context) {
  auto* state = static_cast<ProviderState*>(context);
  ++state->calls;
  return state->value;
}

bool readTextProperty(const void* context, BacnetValue& value) {
  const char* text = static_cast<const char*>(context);
  if (text == nullptr || std::strlen(text) >= sizeof(value.text))
    return false;
  value = BacnetValue{};
  value.type = BacnetValueType::CharacterString;
  value.textLength = std::strlen(text);
  std::memcpy(value.text, text, value.textLength + 1U);
  return true;
}

bool readRealProperty(const void* context, BacnetValue& value) {
  const auto* real = static_cast<const float*>(context);
  if (real == nullptr)
    return false;
  value = BacnetValue{};
  value.type = BacnetValueType::Real;
  value.realValue = *real;
  return true;
}

bool readEnumeratedProperty(const void* context, BacnetValue& value) {
  const auto* enumerated = static_cast<const uint32_t*>(context);
  if (enumerated == nullptr)
    return false;
  value = BacnetValue{};
  value.type = BacnetValueType::Enumerated;
  value.unsignedValue = *enumerated;
  return true;
}

bool equals(float actual, float expected) {
  return actual == expected;
}

struct LegacyAnalogValueLayout {
  uint32_t instance;
  const char* objectName;
  float presentValue;
  uint32_t units;
  bool outOfService;
  BacnetServerAnalogValueProvider presentValueProvider;
  void* presentValueContext;
};

static_assert(std::is_aggregate<BacnetServerAnalogValue>::value,
              "Analog Value must remain aggregate-initializable");
static_assert(sizeof(BacnetServerAnalogValue) == sizeof(LegacyAnalogValueLayout),
              "Baseline Analog Value must not reserve optional-property storage");

bool readProperty(TestTransport& transport,
                  BacnetServer& server,
                  const BacnetIpEndpoint& source,
                  BacnetPropertyRequest request,
                  uint8_t invokeId,
                  BacnetValue& value) {
  uint8_t frame[BacnetProtocol::kMaxReadPropertyRequestSize] = {};
  const size_t frameSize = BacnetProtocol::buildReadPropertyRequest(
    frame, sizeof(frame), request, invokeId);
  if (frameSize == 0) {
    return false;
  }
  transport.queue(frame, frameSize, source);
  return server.poll() == BacnetServerPollResult::ReadPropertyAckSent &&
         BacnetProtocol::parseReadPropertyAck(
           transport.lastSent, transport.lastSentLength, invokeId, request, value);
}

bool requestReadPropertyAck(TestTransport& transport,
                            BacnetServer& server,
                            const BacnetIpEndpoint& source,
                            BacnetPropertyRequest request,
                            uint8_t invokeId) {
  uint8_t frame[BacnetProtocol::kMaxReadPropertyRequestSize] = {};
  const size_t frameSize = BacnetProtocol::buildReadPropertyRequest(
    frame, sizeof(frame), request, invokeId);
  if (frameSize == 0) {
    return false;
  }
  transport.queue(frame, frameSize, source);
  return server.poll() == BacnetServerPollResult::ReadPropertyAckSent;
}

bool readError(TestTransport& transport,
               BacnetServer& server,
               const BacnetIpEndpoint& source,
               BacnetPropertyRequest request,
               uint8_t invokeId,
               uint32_t expectedCode) {
  uint8_t frame[BacnetProtocol::kMaxReadPropertyRequestSize] = {};
  const size_t frameSize = BacnetProtocol::buildReadPropertyRequest(
    frame, sizeof(frame), request, invokeId);
  if (frameSize == 0) {
    return false;
  }
  transport.queue(frame, frameSize, source);
  BacnetValue error;
  uint32_t errorClass = 0;
  uint32_t errorCode = 0;
  return server.poll() == BacnetServerPollResult::ReadPropertyErrorSent &&
         BacnetProtocol::parseReadPropertyError(transport.lastSent,
                                                transport.lastSentLength,
                                                invokeId,
                                                error,
                                                &errorClass,
                                                &errorCode) &&
         errorClass == (expectedCode == 31 ? 1U : 2U) && errorCode == expectedCode;
}

bool propertyListMatches(TestTransport& transport,
                         BacnetServer& server,
                         const BacnetIpEndpoint& source,
                         BacnetObjectId object,
                         const BacnetPropertyId* expected,
                         size_t expectedCount,
                         uint8_t invokeId) {
  BacnetValue value;
  if (!readProperty(transport, server, source,
                    BacnetPropertyRequest{object, BacnetPropertyId::PropertyList, 0},
                    invokeId++, value) ||
      value.type != BacnetValueType::Unsigned ||
      value.unsignedValue != expectedCount) {
    return false;
  }
  for (size_t index = 0; index < expectedCount; ++index) {
    if (!readProperty(transport, server, source,
                      BacnetPropertyRequest{object, BacnetPropertyId::PropertyList,
                                            static_cast<uint32_t>(index + 1U)},
                      invokeId++, value) ||
        value.type != BacnetValueType::Enumerated ||
        value.unsignedValue != static_cast<uint32_t>(expected[index]) ||
        !readProperty(transport, server, source,
                      BacnetPropertyRequest{object, expected[index], kBacnetNoArrayIndex},
                      invokeId++, value)) {
      return false;
    }
  }
  return true;
}

bool testRegisteredAnalogValues() {
  TestTransport transport;
  BacnetServer server(transport);
  BacnetServerDevice device;
  device.deviceInstance = 1234;
  device.objectName = "AV Test Device";
  ProviderState provider{7.25F, 0};
  static constexpr char kProviderDescription[] = "Provider metadata";
  const float providerMinimum = -10.0F;
  const float providerMaximum = 50.0F;
  const float providerResolution = 0.1F;
  const uint32_t providerReliability = 2;
  BacnetServerAnalogValue values[] = {
    {100, "Stored AV", 1.25F, 62, true, nullptr, nullptr},
    {101, "Provider AV", 0.0F, 95, false, readProviderValue, &provider},
  };
  const BacnetObjectId providerObject{
    static_cast<uint16_t>(BacnetObjectType::AnalogValue), values[1].instance};
  const BacnetServerPropertyRegistration properties[] = {
    {providerObject, BacnetPropertyId::Description, readTextProperty, kProviderDescription},
    {providerObject, BacnetPropertyId::MinPresentValue, readRealProperty, &providerMinimum},
    {providerObject, BacnetPropertyId::MaxPresentValue, readRealProperty, &providerMaximum},
    {providerObject, BacnetPropertyId::Resolution, readRealProperty, &providerResolution},
    {providerObject, BacnetPropertyId::Reliability, readEnumeratedProperty, &providerReliability},
  };
  if (!server.setAnalogValues(values, 2) ||
      !server.setPropertyRegistrations(properties, sizeof(properties) / sizeof(properties[0])) ||
      server.analogValueCount() != 2 ||
      !server.begin(device)) {
    return false;
  }

  const BacnetIpEndpoint source(192, 0, 2, 44, 47809);
  const BacnetObjectId deviceObject{
    static_cast<uint16_t>(BacnetObjectType::Device), device.deviceInstance};
  const BacnetObjectId storedObject{
    static_cast<uint16_t>(BacnetObjectType::AnalogValue), values[0].instance};
  BacnetValue value;

  if (!readProperty(transport,
                    server,
                    source,
                    BacnetPropertyRequest{storedObject,
                                          BacnetPropertyId::ObjectIdentifier,
                                          kBacnetNoArrayIndex},
                    1,
                    value) ||
      value.type != BacnetValueType::ObjectIdentifier ||
      value.objectValue.type != storedObject.type ||
      value.objectValue.instance != storedObject.instance) {
    return false;
  }
  if (!readProperty(transport,
                    server,
                    source,
                    BacnetPropertyRequest{storedObject,
                                          BacnetPropertyId::ObjectName,
                                          kBacnetNoArrayIndex},
                    2,
                    value) ||
      std::strcmp(value.displayText(), "Stored AV") != 0) {
    return false;
  }
  if (!readProperty(transport,
                    server,
                    source,
                    BacnetPropertyRequest{storedObject,
                                          BacnetPropertyId::ObjectType,
                                          kBacnetNoArrayIndex},
                    3,
                    value) ||
      value.type != BacnetValueType::Enumerated ||
      value.unsignedValue != static_cast<uint16_t>(BacnetObjectType::AnalogValue)) {
    return false;
  }
  if (!readProperty(transport,
                    server,
                    source,
                    BacnetPropertyRequest{storedObject,
                                          BacnetPropertyId::PresentValue,
                                          kBacnetNoArrayIndex},
                    4,
                    value) ||
      value.type != BacnetValueType::Real || !equals(value.realValue, 1.25F)) {
    return false;
  }
  values[0].presentValue = 2.5F;
  if (!readProperty(transport,
                    server,
                    source,
                    BacnetPropertyRequest{storedObject,
                                          BacnetPropertyId::PresentValue,
                                          kBacnetNoArrayIndex},
                    5,
                    value) ||
      !equals(value.realValue, 2.5F)) {
    return false;
  }
  if (!readProperty(transport,
                    server,
                    source,
                    BacnetPropertyRequest{providerObject,
                                          BacnetPropertyId::PresentValue,
                                          kBacnetNoArrayIndex},
                    6,
                    value) ||
      !equals(value.realValue, 7.25F) || provider.calls != 1) {
    return false;
  }
  provider.value = 8.5F;
  if (!readProperty(transport,
                    server,
                    source,
                    BacnetPropertyRequest{providerObject,
                                          BacnetPropertyId::PresentValue,
                                          kBacnetNoArrayIndex},
                    7,
                    value) ||
      !equals(value.realValue, 8.5F) || provider.calls != 2) {
    return false;
  }

  if (!readProperty(transport,
                    server,
                    source,
                    BacnetPropertyRequest{storedObject,
                                          BacnetPropertyId::StatusFlags,
                                          kBacnetNoArrayIndex},
                    8,
                    value) ||
      value.type != BacnetValueType::BitString || value.bitStringBitCount != 4 ||
      value.bitStringValue != (1UL << 3U)) {
    return false;
  }
  if (!readProperty(transport,
                    server,
                    source,
                    BacnetPropertyRequest{storedObject,
                                          BacnetPropertyId::EventState,
                                          kBacnetNoArrayIndex},
                    9,
                    value) ||
      value.type != BacnetValueType::Enumerated || value.unsignedValue != 0) {
    return false;
  }
  if (!readProperty(transport,
                    server,
                    source,
                    BacnetPropertyRequest{storedObject,
                                          BacnetPropertyId::OutOfService,
                                          kBacnetNoArrayIndex},
                    10,
                    value) ||
      value.type != BacnetValueType::Boolean || !value.booleanValue) {
    return false;
  }
  if (!readProperty(transport,
                    server,
                    source,
                    BacnetPropertyRequest{storedObject,
                                          BacnetPropertyId::Units,
                                          kBacnetNoArrayIndex},
                    11,
                    value) ||
      value.type != BacnetValueType::Enumerated || value.unsignedValue != 62) {
    return false;
  }
  if (!readProperty(transport, server, source,
                    BacnetPropertyRequest{providerObject, BacnetPropertyId::Description,
                                          kBacnetNoArrayIndex}, 36, value) ||
      std::strcmp(value.displayText(), "Provider metadata") != 0 ||
      !readProperty(transport, server, source,
                    BacnetPropertyRequest{providerObject, BacnetPropertyId::MinPresentValue,
                                          kBacnetNoArrayIndex}, 37, value) ||
      value.type != BacnetValueType::Real || !equals(value.realValue, -10.0F) ||
      !readProperty(transport, server, source,
                    BacnetPropertyRequest{providerObject, BacnetPropertyId::Reliability,
                                          kBacnetNoArrayIndex}, 38, value) ||
      value.type != BacnetValueType::Enumerated || value.unsignedValue != 2 ||
      !readProperty(transport, server, source,
                    BacnetPropertyRequest{providerObject, BacnetPropertyId::PropertyList, 0},
                    39, value) || value.unsignedValue != 14) {
    return false;
  }

  static constexpr BacnetPropertyId kProperties[] = {
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
  if (!requestReadPropertyAck(transport,
                              server,
                              source,
                              BacnetPropertyRequest{storedObject,
                                                    BacnetPropertyId::PropertyList,
                                                    kBacnetNoArrayIndex},
                              12) ||
      transport.lastSentLength < 18 ||
      transport.lastSent[transport.lastSentLength - 1U] != 0x3F) {
    return false;
  }
  if (!readProperty(transport,
                    server,
                    source,
                    BacnetPropertyRequest{storedObject, BacnetPropertyId::PropertyList, 0},
                    13,
                    value) ||
      value.type != BacnetValueType::Unsigned || value.unsignedValue != 9) {
    return false;
  }
  for (size_t index = 0; index < sizeof(kProperties) / sizeof(kProperties[0]); ++index) {
    if (!readProperty(transport,
                      server,
                      source,
                      BacnetPropertyRequest{storedObject,
                                            BacnetPropertyId::PropertyList,
                                            static_cast<uint32_t>(index + 1U)},
                      static_cast<uint8_t>(14U + index),
                      value) ||
        value.type != BacnetValueType::Enumerated ||
        value.unsignedValue != static_cast<uint32_t>(kProperties[index])) {
      return false;
    }
  }
  if (!readError(transport,
                 server,
                 source,
                 BacnetPropertyRequest{storedObject, BacnetPropertyId::PropertyList, 10},
                 24,
                 42) ||
      !readError(transport,
                 server,
                 source,
                 BacnetPropertyRequest{storedObject, BacnetPropertyId::Units, 0},
                 25,
                 42) ||
      !readError(transport,
                 server,
                 source,
                 BacnetPropertyRequest{BacnetObjectId{2, 777},
                                       BacnetPropertyId::PresentValue,
                                       kBacnetNoArrayIndex},
                 26,
                 31) ||
      !readError(transport,
                 server,
                 source,
                 BacnetPropertyRequest{storedObject,
                                       static_cast<BacnetPropertyId>(999),
                                       kBacnetNoArrayIndex},
                 27,
                 32)) {
    return false;
  }

  if (!readProperty(transport,
                    server,
                    source,
                    BacnetPropertyRequest{deviceObject,
                                          BacnetPropertyId::ObjectList,
                                          kBacnetNoArrayIndex},
                    28,
                    value) ||
      value.type != BacnetValueType::ObjectIdentifierList ||
      std::strcmp(value.displayText(), "8,1234;2,100;2,101") != 0 ||
      !readProperty(transport,
                    server,
                    source,
                    BacnetPropertyRequest{deviceObject, BacnetPropertyId::ObjectList, 0},
                    29,
                    value) ||
      value.unsignedValue != 3) {
    return false;
  }
  const BacnetObjectId kObjectList[] = {deviceObject, storedObject, providerObject};
  for (size_t index = 0; index < sizeof(kObjectList) / sizeof(kObjectList[0]); ++index) {
    if (!readProperty(transport,
                      server,
                      source,
                      BacnetPropertyRequest{deviceObject,
                                            BacnetPropertyId::ObjectList,
                                            static_cast<uint32_t>(index + 1U)},
                      static_cast<uint8_t>(30U + index),
                      value) ||
        value.type != BacnetValueType::ObjectIdentifier ||
        value.objectValue.type != kObjectList[index].type ||
        value.objectValue.instance != kObjectList[index].instance) {
      return false;
    }
  }
  if (!readProperty(transport,
                    server,
                    source,
                    BacnetPropertyRequest{deviceObject,
                                          BacnetPropertyId::ProtocolObjectTypesSupported,
                                          kBacnetNoArrayIndex},
                    33,
                    value) ||
      value.type != BacnetValueType::BitString || value.bitStringBitCount != 9 ||
      value.bitStringValue != ((1UL << 8U) | (1UL << 2U))) {
    return false;
  }
  if (!readProperty(transport,
                    server,
                    source,
                    BacnetPropertyRequest{deviceObject, BacnetPropertyId::PropertyList, 0},
                    34,
                    value) ||
      value.type != BacnetValueType::Unsigned || value.unsignedValue != 21) {
    return false;
  }

  uint8_t malformed[BacnetProtocol::kMaxReadPropertyRequestSize] = {};
  const size_t malformedSize = BacnetProtocol::buildReadPropertyRequest(
    malformed,
    sizeof(malformed),
    BacnetPropertyRequest{storedObject, BacnetPropertyId::ObjectName, kBacnetNoArrayIndex},
    35);
  const uint32_t sendsBeforeMalformed = transport.sendCalls;
  transport.queue(malformed, malformedSize - 1U, source);
  return malformedSize != 0 && server.poll() == BacnetServerPollResult::Malformed &&
         transport.sendCalls == sendsBeforeMalformed;
}

bool testDisabledAnalogValues() {
  TestTransport transport;
  BacnetServer server(transport);
  BacnetServerDevice device;
  device.deviceInstance = 1234;
  if (!server.setAnalogValues(nullptr, 0) || server.analogValueCount() != 0 ||
      !server.begin(device)) {
    return false;
  }

  const BacnetIpEndpoint source(192, 0, 2, 44, 47809);
  const BacnetObjectId deviceObject{static_cast<uint16_t>(BacnetObjectType::Device), 1234};
  BacnetValue value;
  return readProperty(transport,
                      server,
                      source,
                      BacnetPropertyRequest{deviceObject, BacnetPropertyId::ObjectList, 0},
                      40,
                      value) &&
         value.unsignedValue == 1 &&
         readProperty(transport,
                      server,
                      source,
                      BacnetPropertyRequest{deviceObject,
                                            BacnetPropertyId::ProtocolObjectTypesSupported,
                                            kBacnetNoArrayIndex},
                      41,
                      value) &&
         value.bitStringValue == (1UL << 8U) &&
         readError(transport,
                   server,
                   source,
                   BacnetPropertyRequest{BacnetObjectId{2, 100},
                                         BacnetPropertyId::PresentValue,
                                         kBacnetNoArrayIndex},
                   42,
                   31);
}

bool testIndividuallyRegisteredOptionalProperties() {
  TestTransport transport;
  BacnetServer server(transport);
  BacnetServerDevice device;
  device.deviceInstance = 3456;
  device.objectName = "Optional Property Test Device";
  BacnetServerAnalogValue values[] = {
    {200, "Baseline", 0.0F, 62},
    {201, "Description", 0.0F, 62},
    {202, "Null Description", 0.0F, 62},
    {203, "Empty Description", 0.0F, 62},
    {204, "Minimum", 0.0F, 62},
    {205, "Maximum", 0.0F, 62},
    {206, "Resolution", 0.0F, 62},
    {207, "Reliability", 0.0F, 62},
    {208, "Min Max", 0.0F, 62},
    {209, "Description Resolution", 0.0F, 62},
    {210, "BME Profile", 0.0F, 62},
  };
  const char* nullDescription = nullptr;
  static constexpr char kDescription[] = "Description only";
  static constexpr char kEmptyDescription[] = "";
  static constexpr char kDescriptionResolutionText[] = "Description and resolution";
  static constexpr char kBmeDescription[] = "BME280 temperature";
  const float minimum = -10.0F;
  const float maximum = 50.0F;
  const float resolution = 0.1F;
  const uint32_t reliability = 2;
  BacnetServerPropertyRegistration registrations[11] = {};
  size_t registrationCount = 0;
  const auto object = [&values](size_t index) {
    return BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::AnalogValue),
                          values[index].instance};
  };
  registrations[registrationCount++] = {object(1), BacnetPropertyId::Description,
                                        readTextProperty, kDescription};
  if (nullDescription != nullptr) {
    registrations[registrationCount++] = {object(2), BacnetPropertyId::Description,
                                          readTextProperty, nullDescription};
  }
  registrations[registrationCount++] = {object(3), BacnetPropertyId::Description,
                                        readTextProperty, kEmptyDescription};
  registrations[registrationCount++] = {object(4), BacnetPropertyId::MinPresentValue,
                                        readRealProperty, &minimum};
  registrations[registrationCount++] = {object(5), BacnetPropertyId::MaxPresentValue,
                                        readRealProperty, &maximum};
  registrations[registrationCount++] = {object(6), BacnetPropertyId::Resolution,
                                        readRealProperty, &resolution};
  registrations[registrationCount++] = {object(7), BacnetPropertyId::Reliability,
                                        readEnumeratedProperty, &reliability};
  registrations[registrationCount++] = {object(8), BacnetPropertyId::MinPresentValue,
                                        readRealProperty, &minimum};
  registrations[registrationCount++] = {object(8), BacnetPropertyId::MaxPresentValue,
                                        readRealProperty, &maximum};
  registrations[registrationCount++] = {object(9), BacnetPropertyId::Description,
                                        readTextProperty, kDescriptionResolutionText};
  registrations[registrationCount++] = {object(9), BacnetPropertyId::Resolution,
                                        readRealProperty, &resolution};
  // The complete BME280 profile is registered after the compact combinations.
  BacnetServerPropertyRegistration bmeRegistrations[] = {
    {object(10), BacnetPropertyId::Description, readTextProperty, kBmeDescription},
    {object(10), BacnetPropertyId::MinPresentValue, readRealProperty, &minimum},
    {object(10), BacnetPropertyId::MaxPresentValue, readRealProperty, &maximum},
    {object(10), BacnetPropertyId::Resolution, readRealProperty, &resolution},
    {object(10), BacnetPropertyId::Reliability, readEnumeratedProperty, &reliability},
  };
  // The fixed array above intentionally contains only the compact cases.
  if (!server.setAnalogValues(values, sizeof(values) / sizeof(values[0])) ||
      !server.setPropertyRegistrations(registrations, registrationCount) ||
      !server.begin(device)) {
    return false;
  }
  const BacnetServerPropertyRegistration duplicateRegistrations[] = {
    {object(1), BacnetPropertyId::Description, readTextProperty, kDescription},
    {object(1), BacnetPropertyId::Description, readTextProperty, kDescription},
  };
  if (server.setPropertyRegistrations(duplicateRegistrations,
                                      sizeof(duplicateRegistrations) /
                                        sizeof(duplicateRegistrations[0]))) {
    return false;
  }

  const BacnetIpEndpoint source(192, 0, 2, 44, 47809);
  static constexpr BacnetPropertyId kBase[] = {
    BacnetPropertyId::ObjectIdentifier, BacnetPropertyId::ObjectName,
    BacnetPropertyId::ObjectType, BacnetPropertyId::PresentValue,
    BacnetPropertyId::StatusFlags, BacnetPropertyId::EventState,
    BacnetPropertyId::OutOfService, BacnetPropertyId::Units,
    BacnetPropertyId::PropertyList,
  };
  static constexpr BacnetPropertyId kDescriptionOnly[] = {
    BacnetPropertyId::ObjectIdentifier, BacnetPropertyId::ObjectName,
    BacnetPropertyId::ObjectType, BacnetPropertyId::PresentValue,
    BacnetPropertyId::StatusFlags, BacnetPropertyId::EventState,
    BacnetPropertyId::OutOfService, BacnetPropertyId::Units,
    BacnetPropertyId::PropertyList, BacnetPropertyId::Description,
  };
  static constexpr BacnetPropertyId kMinimumOnly[] = {
    BacnetPropertyId::ObjectIdentifier, BacnetPropertyId::ObjectName,
    BacnetPropertyId::ObjectType, BacnetPropertyId::PresentValue,
    BacnetPropertyId::StatusFlags, BacnetPropertyId::EventState,
    BacnetPropertyId::OutOfService, BacnetPropertyId::Units,
    BacnetPropertyId::PropertyList, BacnetPropertyId::MinPresentValue,
  };
  static constexpr BacnetPropertyId kMaximumOnly[] = {
    BacnetPropertyId::ObjectIdentifier, BacnetPropertyId::ObjectName,
    BacnetPropertyId::ObjectType, BacnetPropertyId::PresentValue,
    BacnetPropertyId::StatusFlags, BacnetPropertyId::EventState,
    BacnetPropertyId::OutOfService, BacnetPropertyId::Units,
    BacnetPropertyId::PropertyList, BacnetPropertyId::MaxPresentValue,
  };
  static constexpr BacnetPropertyId kResolutionOnly[] = {
    BacnetPropertyId::ObjectIdentifier, BacnetPropertyId::ObjectName,
    BacnetPropertyId::ObjectType, BacnetPropertyId::PresentValue,
    BacnetPropertyId::StatusFlags, BacnetPropertyId::EventState,
    BacnetPropertyId::OutOfService, BacnetPropertyId::Units,
    BacnetPropertyId::PropertyList, BacnetPropertyId::Resolution,
  };
  static constexpr BacnetPropertyId kReliabilityOnly[] = {
    BacnetPropertyId::ObjectIdentifier, BacnetPropertyId::ObjectName,
    BacnetPropertyId::ObjectType, BacnetPropertyId::PresentValue,
    BacnetPropertyId::StatusFlags, BacnetPropertyId::EventState,
    BacnetPropertyId::OutOfService, BacnetPropertyId::Units,
    BacnetPropertyId::PropertyList, BacnetPropertyId::Reliability,
  };
  static constexpr BacnetPropertyId kMinMax[] = {
    BacnetPropertyId::ObjectIdentifier, BacnetPropertyId::ObjectName,
    BacnetPropertyId::ObjectType, BacnetPropertyId::PresentValue,
    BacnetPropertyId::StatusFlags, BacnetPropertyId::EventState,
    BacnetPropertyId::OutOfService, BacnetPropertyId::Units,
    BacnetPropertyId::PropertyList, BacnetPropertyId::MinPresentValue,
    BacnetPropertyId::MaxPresentValue,
  };
  static constexpr BacnetPropertyId kDescriptionResolution[] = {
    BacnetPropertyId::ObjectIdentifier, BacnetPropertyId::ObjectName,
    BacnetPropertyId::ObjectType, BacnetPropertyId::PresentValue,
    BacnetPropertyId::StatusFlags, BacnetPropertyId::EventState,
    BacnetPropertyId::OutOfService, BacnetPropertyId::Units,
    BacnetPropertyId::PropertyList, BacnetPropertyId::Description,
    BacnetPropertyId::Resolution,
  };
  const auto check = [&](size_t index, const BacnetPropertyId* expected, size_t count,
                         uint8_t invokeId) {
    return propertyListMatches(transport, server, source, object(index), expected, count,
                               invokeId);
  };
  if (!check(0, kBase, sizeof(kBase) / sizeof(kBase[0]), 1) ||
      !check(1, kDescriptionOnly, sizeof(kDescriptionOnly) / sizeof(kDescriptionOnly[0]), 40) ||
      !check(2, kBase, sizeof(kBase) / sizeof(kBase[0]), 80) ||
      !check(3, kDescriptionOnly, sizeof(kDescriptionOnly) / sizeof(kDescriptionOnly[0]), 120) ||
      !check(4, kMinimumOnly, sizeof(kMinimumOnly) / sizeof(kMinimumOnly[0]), 160) ||
      !check(5, kMaximumOnly, sizeof(kMaximumOnly) / sizeof(kMaximumOnly[0]), 200) ||
      !check(6, kResolutionOnly, sizeof(kResolutionOnly) / sizeof(kResolutionOnly[0]), 20) ||
      !check(7, kReliabilityOnly, sizeof(kReliabilityOnly) / sizeof(kReliabilityOnly[0]), 60) ||
      !check(8, kMinMax, sizeof(kMinMax) / sizeof(kMinMax[0]), 100) ||
      !check(9, kDescriptionResolution,
             sizeof(kDescriptionResolution) / sizeof(kDescriptionResolution[0]), 150) ||
      !readError(transport, server, source,
                 BacnetPropertyRequest{object(2), BacnetPropertyId::Description,
                                       kBacnetNoArrayIndex}, 210, 32)) {
    return false;
  }
  const BacnetPropertyId optionalProperties[] = {
    BacnetPropertyId::Description,
    BacnetPropertyId::MinPresentValue,
    BacnetPropertyId::MaxPresentValue,
    BacnetPropertyId::Resolution,
    BacnetPropertyId::Reliability,
  };
  const auto contains = [](const BacnetPropertyId* list, size_t count,
                           BacnetPropertyId property) {
    for (size_t index = 0; index < count; ++index) {
      if (list[index] == property)
        return true;
    }
    return false;
  };
  struct OptionalCase {
    size_t objectIndex;
    const BacnetPropertyId* properties;
    size_t propertyCount;
  };
  const OptionalCase cases[] = {
    {0, kBase, sizeof(kBase) / sizeof(kBase[0])},
    {1, kDescriptionOnly, sizeof(kDescriptionOnly) / sizeof(kDescriptionOnly[0])},
    {2, kBase, sizeof(kBase) / sizeof(kBase[0])},
    {3, kDescriptionOnly, sizeof(kDescriptionOnly) / sizeof(kDescriptionOnly[0])},
    {4, kMinimumOnly, sizeof(kMinimumOnly) / sizeof(kMinimumOnly[0])},
    {5, kMaximumOnly, sizeof(kMaximumOnly) / sizeof(kMaximumOnly[0])},
    {6, kResolutionOnly, sizeof(kResolutionOnly) / sizeof(kResolutionOnly[0])},
    {7, kReliabilityOnly, sizeof(kReliabilityOnly) / sizeof(kReliabilityOnly[0])},
    {8, kMinMax, sizeof(kMinMax) / sizeof(kMinMax[0])},
    {9, kDescriptionResolution,
     sizeof(kDescriptionResolution) / sizeof(kDescriptionResolution[0])},
  };
  uint8_t errorInvokeId = 212;
  for (const OptionalCase& optionalCase : cases) {
    for (BacnetPropertyId property : optionalProperties) {
      if (!contains(optionalCase.properties, optionalCase.propertyCount, property) &&
          !readError(transport, server, source,
                     BacnetPropertyRequest{object(optionalCase.objectIndex), property,
                                           kBacnetNoArrayIndex}, errorInvokeId++, 32)) {
        return false;
      }
    }
  }
  BacnetValue emptyDescription;
  if (!readProperty(transport, server, source,
                    BacnetPropertyRequest{object(3), BacnetPropertyId::Description,
                                          kBacnetNoArrayIndex}, 211, emptyDescription) ||
      emptyDescription.type != BacnetValueType::CharacterString ||
      emptyDescription.textLength != 0) {
    return false;
  }

  // A separate complete BME profile validates all five individually registered
  // optional properties without adding storage to baseline AV entries.
  if (!server.setPropertyRegistrations(bmeRegistrations,
                                       sizeof(bmeRegistrations) / sizeof(bmeRegistrations[0]))) {
    return false;
  }
  static constexpr BacnetPropertyId kBmeProfile[] = {
    BacnetPropertyId::ObjectIdentifier, BacnetPropertyId::ObjectName,
    BacnetPropertyId::ObjectType, BacnetPropertyId::PresentValue,
    BacnetPropertyId::StatusFlags, BacnetPropertyId::EventState,
    BacnetPropertyId::OutOfService, BacnetPropertyId::Units,
    BacnetPropertyId::PropertyList, BacnetPropertyId::Description,
    BacnetPropertyId::MinPresentValue, BacnetPropertyId::MaxPresentValue,
    BacnetPropertyId::Resolution, BacnetPropertyId::Reliability,
  };
  return check(10, kBmeProfile, sizeof(kBmeProfile) / sizeof(kBmeProfile[0]), 1) &&
         readError(transport, server, source,
                   BacnetPropertyRequest{object(10), static_cast<BacnetPropertyId>(999),
                                         kBacnetNoArrayIndex}, 100, 32);
}

bool testStartConfigurationAndVersionFallback() {
  BacnetServerDevice device;
  device.deviceInstance = 1234;
  device.objectName = "Server Test Device";
  device.firmwareRevision = "9.9.9";
  const BacnetIpEndpoint source(192, 0, 2, 44, 47809);
  const BacnetObjectId deviceObject{static_cast<uint16_t>(BacnetObjectType::Device),
                                    device.deviceInstance};
  BacnetValue value;

  TestTransport defaultTransport;
  BacnetServer defaultServer(defaultTransport);
  if (!defaultServer.begin(device) ||
      defaultTransport.beginCalls != 1 ||
      defaultTransport.lastBeginPort != BacnetServer::kDefaultPort ||
      defaultServer.port() != BacnetServer::kDefaultPort ||
      !readProperty(defaultTransport,
                    defaultServer,
                    source,
                    BacnetPropertyRequest{deviceObject,
                                          BacnetPropertyId::ApplicationSoftwareVersion,
                                          kBacnetNoArrayIndex},
                    50,
                    value) ||
      value.type != BacnetValueType::CharacterString ||
      std::strcmp(value.displayText(), "9.9.9") != 0) {
    return false;
  }
  defaultServer.end();

  device.applicationSoftwareVersion = "";
  TestTransport customTransport;
  BacnetServer customServer(customTransport);
  constexpr uint16_t kCustomPort = 47809;
  if (!customServer.begin(device, kCustomPort) ||
      customTransport.beginCalls != 1 ||
      customTransport.lastBeginPort != kCustomPort ||
      customServer.port() != kCustomPort ||
      !readProperty(customTransport,
                    customServer,
                    source,
                    BacnetPropertyRequest{deviceObject,
                                          BacnetPropertyId::ApplicationSoftwareVersion,
                                          kBacnetNoArrayIndex},
                    51,
                    value) ||
      value.type != BacnetValueType::CharacterString ||
      std::strcmp(value.displayText(), "9.9.9") != 0) {
    return false;
  }
  customServer.end();
  return true;
}

bool testLimitStates() {
  const BacnetAnalogValueLimitConfig limits{-40.0F, -5.0F, 25.0F, 40.0F, 0.5F};
  return bacnetAnalogValueLimitConfigIsValid(limits) &&
         bacnetAnalogValueLimitEvaluate(10.0F, true, limits).state ==
           BacnetAnalogValueLimitState::Normal &&
         bacnetAnalogValueLimitEvaluate(-6.0F, true, limits).state ==
           BacnetAnalogValueLimitState::LowWarning &&
         bacnetAnalogValueLimitEvaluate(26.0F, true, limits).state ==
           BacnetAnalogValueLimitState::HighWarning &&
         bacnetAnalogValueLimitEvaluate(-41.0F, true, limits).fault &&
         bacnetAnalogValueLimitEvaluate(41.0F, true, limits).fault &&
         bacnetAnalogValueLimitEvaluate(0.0F, false, limits).state ==
           BacnetAnalogValueLimitState::SensorFault &&
         bacnetAnalogValueLimitEvaluate(-4.8F, true, limits,
           BacnetAnalogValueLimitState::LowWarning).state ==
           BacnetAnalogValueLimitState::Normal &&
         !bacnetAnalogValueLimitConfigIsValid({0.0F, 10.0F, 5.0F, 20.0F, 0.0F});
}

bool readStatusFlagsProperty(const void* context, BacnetValue& value) {
  const auto* flags = static_cast<const uint32_t*>(context);
  if (flags == nullptr)
    return false;
  value = BacnetValue{};
  value.type = BacnetValueType::BitString;
  value.bitStringValue = *flags;
  value.bitStringBitCount = 4;
  return true;
}

bool testReadOnlyInputObjects() {
  TestTransport transport;
  BacnetServer server(transport);
  BacnetServerDevice device;
  device.deviceInstance = 4567;
  BacnetServerAnalogInput analogInputs[] = {{0, "Light Sensor", 42.5F, BacnetEngineeringUnits::Percent},
                                             {1, "Temperature", 20.0F, BacnetEngineeringUnits::DegreesCelsius}};
  BacnetServerBinaryInput binaryInputs[] = {{0, "Reset Button", true},
                                             {1, "Mid Button", false},
                                             {2, "Set Button", true}};
  const uint32_t noSensor = 1;
  const uint32_t faultEvent = 1;
  const uint32_t faultFlags = 1UL << 1U;
  const BacnetObjectId temperatureObject{static_cast<uint16_t>(BacnetObjectType::AnalogInput), 1};
  const BacnetObjectId resetObject{static_cast<uint16_t>(BacnetObjectType::BinaryInput), 0};
  const BacnetServerPropertyRegistration properties[] = {
    {temperatureObject, BacnetPropertyId::Reliability, readEnumeratedProperty, &noSensor},
    {temperatureObject, BacnetPropertyId::EventState, readEnumeratedProperty, &faultEvent},
    {temperatureObject, BacnetPropertyId::StatusFlags, readStatusFlagsProperty, &faultFlags},
  };
  if (!server.setAnalogInputs(analogInputs, 2) || !server.setBinaryInputs(binaryInputs, 3) ||
      !server.setPropertyRegistrations(properties, 3) || !server.begin(device)) {
    return false;
  }
  const BacnetIpEndpoint source(192, 0, 2, 44, 47809);
  BacnetValue value;
  const BacnetObjectId lightObject{static_cast<uint16_t>(BacnetObjectType::AnalogInput), 0};
  if (!readProperty(transport, server, source,
                    {lightObject, BacnetPropertyId::PresentValue, kBacnetNoArrayIndex}, 1, value) ||
      value.type != BacnetValueType::Real || !equals(value.realValue, 42.5F) ||
      !readProperty(transport, server, source,
                    {lightObject, BacnetPropertyId::Units, kBacnetNoArrayIndex}, 2, value) ||
      value.unsignedValue != BacnetEngineeringUnits::Percent ||
      !readProperty(transport, server, source,
                    {resetObject, BacnetPropertyId::PresentValue, kBacnetNoArrayIndex}, 3, value) ||
      value.type != BacnetValueType::Enumerated || value.unsignedValue != 1 ||
      !readProperty(transport, server, source,
                    {temperatureObject, BacnetPropertyId::Reliability, kBacnetNoArrayIndex}, 4, value) ||
      value.unsignedValue != noSensor ||
      !readProperty(transport, server, source,
                    {temperatureObject, BacnetPropertyId::EventState, kBacnetNoArrayIndex}, 5, value) ||
      value.unsignedValue != faultEvent ||
      !readProperty(transport, server, source,
                    {temperatureObject, BacnetPropertyId::StatusFlags, kBacnetNoArrayIndex}, 6, value) ||
      value.type != BacnetValueType::BitString || value.bitStringValue != faultFlags ||
      value.bitStringBitCount != 4) {
    return false;
  }
  return readProperty(transport, server, source,
                      {BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::Device), device.deviceInstance},
                       BacnetPropertyId::ObjectList, 0}, 7, value) &&
         value.unsignedValue == 6;
}

struct OutputApplyState {
  uint32_t calls = 0;
  bool presentValue = false;
  bool outOfService = false;
};

void applyOutput(void* context, bool presentValue, bool outOfService) {
  auto* state = static_cast<OutputApplyState*>(context);
  if (state == nullptr)
    return;
  ++state->calls;
  state->presentValue = presentValue;
  state->outOfService = outOfService;
}

bool writeOutput(TestTransport& transport,
                 BacnetServer& server,
                 const BacnetIpEndpoint& source,
                 BacnetObjectId object,
                 BacnetPropertyId property,
                 const BacnetValue& value,
                 const BacnetWritePropertyOptions& options,
                 uint8_t invokeId) {
  uint8_t frame[BacnetProtocol::kMaxWritePropertyRequestSize] = {};
  const size_t frameSize = BacnetProtocol::buildWritePropertyRequest(
    frame, sizeof(frame), object, property, value, options, invokeId);
  if (frameSize == 0)
    return false;
  transport.queue(frame, frameSize, source);
  return server.poll() == BacnetServerPollResult::WritePropertyAckSent &&
         BacnetProtocol::classifyWritePropertyResponse(transport.lastSent,
                                                        transport.lastSentLength,
                                                        invokeId) ==
           BacnetWritePropertyResponseKind::Ack;
}

bool writeOutputError(TestTransport& transport,
                      BacnetServer& server,
                      const BacnetIpEndpoint& source,
                      BacnetObjectId object,
                      BacnetPropertyId property,
                      const BacnetValue& value,
                      const BacnetWritePropertyOptions& options,
                      uint8_t invokeId,
                      uint8_t expectedCode) {
  uint8_t frame[BacnetProtocol::kMaxWritePropertyRequestSize] = {};
  const size_t frameSize = BacnetProtocol::buildWritePropertyRequest(
    frame, sizeof(frame), object, property, value, options, invokeId);
  if (frameSize == 0)
    return false;
  transport.queue(frame, frameSize, source);
  return server.poll() == BacnetServerPollResult::WritePropertyErrorSent &&
         transport.lastSentLength >= 13 && transport.lastSent[6] == 0x50 &&
         transport.lastSent[7] == invokeId && transport.lastSent[8] == 0x0F &&
         transport.lastSent[12] == expectedCode;
}

bool testCommandableBinaryOutputs() {
  TestTransport transport;
  BacnetServer server(transport);
  OutputApplyState applied;
  BacnetServerBinaryOutput outputs[] = {{0, "LED 1"}};
  outputs[0].priority.relinquishDefault = false;
  outputs[0].apply = applyOutput;
  outputs[0].applyContext = &applied;
  BacnetServerDevice device;
  device.deviceInstance = 7890;
  if (!server.setBinaryOutputs(outputs, 1) || !server.begin(device))
    return false;
  const BacnetIpEndpoint source(192, 0, 2, 44, 47809);
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::BinaryOutput), 0};
  BacnetValue value;
  if (!readProperty(transport, server, source,
                    {object, BacnetPropertyId::PresentValue, kBacnetNoArrayIndex}, 1,
                    value) || value.type != BacnetValueType::Enumerated || value.unsignedValue != 0 ||
      !readProperty(transport, server, source,
                    {object, BacnetPropertyId::PriorityArray, 0}, 2, value) ||
      value.type != BacnetValueType::Unsigned ||
      value.unsignedValue != BacnetPriorityArray::kSlotCount ||
      !readProperty(transport, server, source,
                    {object, BacnetPropertyId::PriorityArray, 16}, 3, value) ||
      value.type != BacnetValueType::Null) {
    return false;
  }
  BacnetValue active;
  active.type = BacnetValueType::Enumerated;
  active.unsignedValue = 1;
  BacnetValue inactive;
  inactive.type = BacnetValueType::Enumerated;
  inactive.unsignedValue = 0;
  BacnetValue nullValue;
  nullValue.type = BacnetValueType::Null;
  BacnetWritePropertyOptions priority16;
  priority16.hasPriority = true;
  priority16.priority = 16;
  BacnetWritePropertyOptions priority8;
  priority8.hasPriority = true;
  priority8.priority = 8;
  BacnetWritePropertyOptions noPriority;
  if (!writeOutput(transport, server, source, object, BacnetPropertyId::PresentValue,
                   active, noPriority, 4) || !applied.presentValue ||
      !writeOutput(transport, server, source, object, BacnetPropertyId::PresentValue,
                   nullValue, priority16, 5) || applied.presentValue ||
      !writeOutput(transport, server, source, object, BacnetPropertyId::PresentValue,
                   active, priority16, 6) || !applied.presentValue || applied.outOfService ||
      !writeOutput(transport, server, source, object, BacnetPropertyId::PresentValue,
                   inactive, priority8, 7) || applied.presentValue ||
      !writeOutput(transport, server, source, object, BacnetPropertyId::PresentValue,
                   active, priority16, 8) || applied.presentValue ||
      !writeOutput(transport, server, source, object, BacnetPropertyId::PresentValue,
                   nullValue, priority8, 9) || !applied.presentValue ||
      !writeOutput(transport, server, source, object, BacnetPropertyId::PresentValue,
                   nullValue, priority16, 10) || applied.presentValue ||
      !readProperty(transport, server, source,
                    {object, BacnetPropertyId::PresentValue, kBacnetNoArrayIndex}, 11,
                    value) || value.unsignedValue != 0) {
    return false;
  }
  BacnetWritePropertyOptions invalidPriority;
  invalidPriority.hasPriority = true;
  invalidPriority.priority = 1;
  uint8_t invalidFrame[BacnetProtocol::kMaxWritePropertyRequestSize] = {};
  size_t invalidSize = BacnetProtocol::buildWritePropertyRequest(
    invalidFrame, sizeof(invalidFrame), object, BacnetPropertyId::PresentValue,
    active, invalidPriority, 12);
  if (invalidSize == 0)
    return false;
  invalidFrame[invalidSize - 1U] = 0;
  transport.queue(invalidFrame, invalidSize, source);
  if (server.poll() != BacnetServerPollResult::WritePropertyErrorSent ||
      transport.lastSentLength < 13 || transport.lastSent[12] != 37) {
    return false;
  }
  BacnetValue invalidType;
  invalidType.type = BacnetValueType::Real;
  invalidType.realValue = 1.0F;
  if (!writeOutputError(transport, server, source, object,
                        BacnetPropertyId::PresentValue, invalidType, priority16, 13, 9) ||
      !writeOutputError(transport, server, source, object,
                        BacnetPropertyId::PriorityArray, active, priority16, 14, 40)) {
    return false;
  }
  BacnetValue outOfService;
  outOfService.type = BacnetValueType::Boolean;
  outOfService.booleanValue = true;
  if (!writeOutput(transport, server, source, object, BacnetPropertyId::OutOfService,
                   outOfService, noPriority, 15) || !applied.outOfService ||
      !readProperty(transport, server, source,
                    {object, BacnetPropertyId::OutOfService, kBacnetNoArrayIndex}, 16,
                    value) || !value.booleanValue) {
    return false;
  }
  return true;
}

} // namespace

int main() {
  return testRegisteredAnalogValues() && testDisabledAnalogValues() &&
           testIndividuallyRegisteredOptionalProperties() &&
           testStartConfigurationAndVersionFallback() && testLimitStates()
           && testReadOnlyInputObjects() && testCommandableBinaryOutputs()
           ? 0
           : 1;
}
