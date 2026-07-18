// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetServer.h"
#include "portable/BacnetAnalogValueLimits.h"

#include <cstring>

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

bool equals(float actual, float expected) {
  return actual == expected;
}

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

bool testRegisteredAnalogValues() {
  TestTransport transport;
  BacnetServer server(transport);
  BacnetServerDevice device;
  device.deviceInstance = 1234;
  device.objectName = "AV Test Device";
  ProviderState provider{7.25F, 0};
  BacnetServerAnalogValueMetadata providerMetadata{
    "Provider metadata", -10.0F, 50.0F, 0.1F, 3, 2, true, true};
  BacnetServerAnalogValue values[] = {
    {100, "Stored AV", 1.25F, 62, true, nullptr, nullptr},
    {101, "Provider AV", 0.0F, 95, false, readProviderValue, &provider, &providerMetadata},
  };
  if (!server.setAnalogValues(values, 2) || server.analogValueCount() != 2 ||
      !server.begin(device)) {
    return false;
  }

  const BacnetIpEndpoint source(192, 0, 2, 44, 47809);
  const BacnetObjectId deviceObject{
    static_cast<uint16_t>(BacnetObjectType::Device), device.deviceInstance};
  const BacnetObjectId storedObject{
    static_cast<uint16_t>(BacnetObjectType::AnalogValue), values[0].instance};
  const BacnetObjectId providerObject{
    static_cast<uint16_t>(BacnetObjectType::AnalogValue), values[1].instance};
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

} // namespace

int main() {
  return testRegisteredAnalogValues() && testDisabledAnalogValues() &&
           testStartConfigurationAndVersionFallback() && testLimitStates()
           ? 0
           : 1;
}
