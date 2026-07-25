// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetClient.h"
#include "BacnetServer.h"
#include "platform/windows/WindowsBacnetDatagramTransport.h"
#include "platform/windows/WindowsSocketRuntime.h"

#include <cstdio>
#include <cstring>
#include <thread>

namespace {

constexpr uint16_t kServerPort = 47808;
constexpr uint16_t kClientPort = 47809;
constexpr uint32_t kDeviceInstance = 1682127;
constexpr uint16_t kVendorId = 555;
constexpr size_t kPumpAttempts = 256;

int failures = 0;

bool expect(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "[E] %s\n", message);
    ++failures;
  }
  return condition;
}

struct ProviderState {
  float value = 0.0F;
  uint32_t calls = 0;
};

float readCallbackValue(void* context) {
  auto* state = static_cast<ProviderState*>(context);
  if (state == nullptr) {
    return 0.0F;
  }
  ++state->calls;
  return state->value;
}

bool readTextProperty(const void* context, BacnetValue& value) {
  const auto* text = static_cast<const char*>(context);
  if (text == nullptr) {
    return false;
  }
  const size_t length = std::strlen(text);
  if (length >= BacnetValue::kMaxTextLength) {
    return false;
  }
  value = BacnetValue{};
  std::memcpy(value.text, text, length + 1U);
  value.textLength = length;
  value.type = BacnetValueType::CharacterString;
  return true;
}

template <typename Predicate>
bool pumpServerUntil(BacnetServer& server, Predicate predicate) {
  for (size_t attempt = 0; attempt < kPumpAttempts; ++attempt) {
    static_cast<void>(server.poll());
    if (predicate()) {
      return true;
    }
    std::this_thread::yield();
  }
  return false;
}

bool readProperty(BacnetClient& client,
                  BacnetServer& server,
                  const BacnetIpEndpoint& target,
                  const BacnetPropertyRequest& request,
                  uint8_t invokeId,
                  BacnetValue& value) {
  if (!client.sendReadProperty(target, request, invokeId)) {
    std::fprintf(stderr, "[E] ReadProperty send failed for property %u\n",
                 static_cast<unsigned>(request.property));
    return false;
  }

  BacnetReadPropertyPollStatus status = BacnetReadPropertyPollStatus::None;
  const bool received = pumpServerUntil(server, [&] {
    status = client.pollReadPropertyStatus(value, invokeId, request);
    return status != BacnetReadPropertyPollStatus::None;
  });
  if (!received || status != BacnetReadPropertyPollStatus::Ack) {
    std::fprintf(stderr,
                 "[E] ReadProperty failed for property %u (received=%s, status=%u)\n",
                 static_cast<unsigned>(request.property),
                 received ? "yes" : "no", static_cast<unsigned>(status));
    return false;
  }
  return true;
}

bool readPropertyError(BacnetClient& client,
                       BacnetServer& server,
                       const BacnetIpEndpoint& target,
                       const BacnetPropertyRequest& request,
                       uint8_t invokeId,
                       uint32_t expectedErrorClass = 0,
                       uint32_t expectedErrorCode = 0) {
  if (!client.sendReadProperty(target, request, invokeId)) {
    return false;
  }
  BacnetValue value;
  BacnetReadPropertyPollStatus status = BacnetReadPropertyPollStatus::None;
  uint32_t errorClass = 0;
  uint32_t errorCode = 0;
  const bool received = pumpServerUntil(server, [&] {
    status = client.pollReadPropertyStatus(
      value, invokeId, request, &errorClass, &errorCode);
    return status != BacnetReadPropertyPollStatus::None;
  });
  return received && status == BacnetReadPropertyPollStatus::Error &&
         (expectedErrorClass == 0 ||
          (errorClass == expectedErrorClass && errorCode == expectedErrorCode));
}

bool readProperty(BacnetClient& client,
                  BacnetServer& server,
                  const BacnetIpEndpoint& target,
                  BacnetObjectId object,
                  BacnetPropertyId property,
                  uint8_t invokeId,
                  BacnetValue& value,
                  uint32_t arrayIndex = kBacnetNoArrayIndex) {
  return readProperty(client,
                      server,
                      target,
                      BacnetPropertyRequest{object, property, arrayIndex},
                      invokeId,
                      value);
}

bool matchesObject(const BacnetObjectId& object, uint16_t type, uint32_t instance) {
  return object.type == type && object.instance == instance;
}

void testLoopbackServerClientPath() {
  // This is a test-only Windows harness. It is not a Windows server API.
  WindowsSocketRuntime runtime;
  WindowsBacnetDatagramTransport serverTransport(runtime);
  WindowsBacnetDatagramTransport clientTransport(runtime);
  BacnetServer server(serverTransport);
  BacnetClient client(clientTransport);
  ProviderState callbackState{23.5F, 0};
  BacnetServerAnalogValue analogValues[] = {
    {200, "Stored Analog Value", 12.5F, 62, false, nullptr, nullptr},
    {201, "Callback Analog Value", 0.0F, 73, false, readCallbackValue, &callbackState},
  };
  constexpr const char* stateText[] = {"Off", "Auto", "On"};
  constexpr const char* alternateStateText[] = {"Stop", "Run"};
  BacnetServerMultiStateValue multiStateValues[] = {
    {2020, "Loopback Operating Mode", 2, 3, stateText, false},
    {2021, "Loopback Pump Mode", 1, 2, alternateStateText, false},
  };
  const BacnetServerDevice device{
    kDeviceInstance,
    kVendorId,
    "Windows Loopback Test Device",
    "Unregistered BACnet Test Server",
    "Windows Test Harness",
    "0.35.0",
    "0.35.0",
  };
  const BacnetIpEndpoint loopback(127, 0, 0, 1, kServerPort);
  const BacnetObjectId deviceObject{
    static_cast<uint16_t>(BacnetObjectType::Device), kDeviceInstance};
  const BacnetObjectId storedObject{
    static_cast<uint16_t>(BacnetObjectType::AnalogValue), 200};
  const BacnetObjectId callbackObject{
    static_cast<uint16_t>(BacnetObjectType::AnalogValue), 201};
  const BacnetObjectId multiStateObject{
    static_cast<uint16_t>(BacnetObjectType::MultiStateValue), 2020};
  const BacnetObjectId alternateMultiStateObject{
    static_cast<uint16_t>(BacnetObjectType::MultiStateValue), 2021};
  static constexpr char kRegisteredMsvName[] = "Registered Operating Mode";
  static constexpr char kMsvDescription[] = "Operating mode selected by the demo";
  const BacnetServerPropertyRegistration msvProperties[] = {
    {multiStateObject, BacnetPropertyId::ObjectName, readTextProperty, kRegisteredMsvName},
    {multiStateObject, BacnetPropertyId::Description, readTextProperty, kMsvDescription},
  };

  if (!expect(runtime.begin(), "Winsock runtime failed") ||
      !expect(serverTransport.setBindAddress(loopback),
              "server loopback bind configuration failed") ||
      !expect(clientTransport.setBindAddress(
                BacnetIpEndpoint(127, 0, 0, 1, kClientPort)),
              "client loopback bind configuration failed") ||
      !expect(server.setAnalogValues(analogValues, 2),
              "server Analog Value configuration failed") ||
      !expect(server.setMultiStateValues(multiStateValues, 2),
              "server Multi-state Value configuration failed") ||
      !expect(server.setPropertyRegistrations(
                msvProperties, sizeof(msvProperties) / sizeof(msvProperties[0])),
              "server Multi-state Value property registration failed") ||
      !expect(server.begin(device, kServerPort), "server loopback bind failed") ||
      !expect(client.begin(kClientPort), "client loopback bind failed")) {
    client.end();
    server.end();
    runtime.end();
    return;
  }

  expect(serverTransport.localEndpoint().port == kServerPort,
         "server must bind UDP port 47808") &&
    expect(clientTransport.localEndpoint().port == kClientPort,
           "client must bind its separate UDP port 47809");

  BacnetIAmDevice discovered;
  const bool whoIsSent = client.sendWhoIs(loopback);
  const bool iamReceived = whoIsSent && pumpServerUntil(server, [&] {
    return client.pollIAm(discovered);
  });
  expect(whoIsSent, "unicast Who-Is send failed");
  expect(iamReceived, "I-Am was not received on the client source port") &&
    expect(discovered.endpoint.port == kServerPort,
           "I-Am source port must be the server BACnet port") &&
    expect(discovered.deviceInstance == kDeviceInstance,
           "I-Am device instance mismatch") &&
    expect(discovered.vendorId == kVendorId, "I-Am vendor ID mismatch");

  const BacnetPropertyId deviceProperties[] = {
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
    BacnetPropertyId::MaxApduLengthAccepted,
    BacnetPropertyId::SegmentationSupported,
    BacnetPropertyId::ApduTimeout,
    BacnetPropertyId::NumberOfApduRetries,
    BacnetPropertyId::DeviceAddressBinding,
    BacnetPropertyId::DatabaseRevision,
  };
  BacnetValue value;
  uint8_t invokeId = 10;
  for (const BacnetPropertyId property : deviceProperties) {
    expect(readProperty(client, server, loopback, deviceObject, property, invokeId, value),
           "mandatory Device property read failed");
    ++invokeId;
  }
  expect(readProperty(client,
                      server,
                      loopback,
                      deviceObject,
                      BacnetPropertyId::ObjectList,
                      invokeId++,
                      value) &&
           value.type == BacnetValueType::ObjectIdentifierList,
         "Device Object List full read failed");
  expect(readProperty(client,
                      server,
                      loopback,
                      deviceObject,
                      BacnetPropertyId::VendorIdentifier,
                      invokeId++,
                      value) &&
           value.type == BacnetValueType::Unsigned && value.unsignedValue == kVendorId,
         "Device Vendor Identifier mismatch");

  expect(readProperty(client,
                      server,
                      loopback,
                      deviceObject,
                      BacnetPropertyId::PropertyList,
                      invokeId++,
                      value) &&
           value.type == BacnetValueType::PropertyIdentifierList,
         "Device Property List full read failed");
  expect(readProperty(client,
                      server,
                      loopback,
                      deviceObject,
                      BacnetPropertyId::ProtocolObjectTypesSupported,
                      invokeId++,
                      value) &&
           value.type == BacnetValueType::BitString && value.bitStringBitCount == 20 &&
           (value.bitStringValue &
            (1UL << static_cast<uint16_t>(BacnetObjectType::MultiStateValue))) != 0,
         "Device Protocol Object Types must advertise MSV with a 20-bit bitmap");
  expect(readProperty(client,
                      server,
                      loopback,
                      deviceObject,
                      BacnetPropertyId::ObjectList,
                      invokeId++,
                      value,
                      0) &&
           value.type == BacnetValueType::Unsigned && value.unsignedValue == 5,
         "Device Object List count must contain Device, two Analog Values, and both MSVs");
  const BacnetObjectId expectedObjectList[] = {
    deviceObject, storedObject, callbackObject, multiStateObject, alternateMultiStateObject};
  for (size_t index = 0; index < 5; ++index) {
    expect(readProperty(client,
                        server,
                        loopback,
                        deviceObject,
                        BacnetPropertyId::ObjectList,
                        invokeId++,
                        value,
                        static_cast<uint32_t>(index + 1U)) &&
             value.type == BacnetValueType::ObjectIdentifier &&
             matchesObject(value.objectValue,
                           expectedObjectList[index].type,
                           expectedObjectList[index].instance),
           "Device Object List entry mismatch");
  }
  expect(readProperty(client,
                      server,
                      loopback,
                      deviceObject,
                      BacnetPropertyId::PropertyList,
                      invokeId++,
                      value,
                      0) &&
           value.type == BacnetValueType::Unsigned && value.unsignedValue == 21,
         "Device Property List count mismatch");

  const BacnetPropertyId analogValueProperties[] = {
    BacnetPropertyId::ObjectIdentifier,
    BacnetPropertyId::ObjectName,
    BacnetPropertyId::ObjectType,
    BacnetPropertyId::PresentValue,
    BacnetPropertyId::StatusFlags,
    BacnetPropertyId::EventState,
    BacnetPropertyId::OutOfService,
    BacnetPropertyId::Units,
  };
  for (size_t objectIndex = 1; objectIndex < 3; ++objectIndex) {
    const BacnetObjectId object = expectedObjectList[objectIndex];
    for (const BacnetPropertyId property : analogValueProperties) {
      expect(readProperty(client, server, loopback, object, property, invokeId++, value),
             "Analog Value property read failed");
    }
    expect(readProperty(client,
                        server,
                        loopback,
                        object,
                        BacnetPropertyId::PropertyList,
                        invokeId++,
                        value) &&
             value.type == BacnetValueType::PropertyIdentifierList,
           "Analog Value Property List full read failed");
    expect(readProperty(client,
                        server,
                        loopback,
                        object,
                        BacnetPropertyId::PropertyList,
                        invokeId++,
                        value,
                        0) &&
             value.type == BacnetValueType::Unsigned && value.unsignedValue == 9,
           "Analog Value Property List count mismatch");
  }

  expect(readProperty(client,
                      server,
                      loopback,
                      storedObject,
                      BacnetPropertyId::PresentValue,
                      invokeId++,
                      value) &&
           value.type == BacnetValueType::Real && value.realValue == 12.5F,
         "stored Analog Value Present Value mismatch");
  expect(readProperty(client,
                      server,
                      loopback,
                      callbackObject,
                      BacnetPropertyId::PresentValue,
                      invokeId++,
                      value) &&
           value.type == BacnetValueType::Real && value.realValue == callbackState.value &&
           callbackState.calls != 0,
         "callback Analog Value Present Value mismatch");
  expect(readProperty(client,
                      server,
                      loopback,
                      multiStateObject,
                      BacnetPropertyId::PresentValue,
                      invokeId++,
                      value) &&
           value.type == BacnetValueType::Unsigned && value.unsignedValue == 2,
         "MSV2020 Present Value mismatch");
  expect(readProperty(client,
                      server,
                      loopback,
                      alternateMultiStateObject,
                      BacnetPropertyId::PresentValue,
                      invokeId++,
                      value) &&
           value.type == BacnetValueType::Unsigned && value.unsignedValue == 1,
         "MSV2021 Present Value mismatch");
  expect(readProperty(client,
                      server,
                      loopback,
                      multiStateObject,
                      BacnetPropertyId::NumberOfStates,
                      invokeId++,
                      value) &&
           value.type == BacnetValueType::Unsigned && value.unsignedValue == 3,
         "MSV2020 Number Of States mismatch");
  expect(readProperty(client,
                      server,
                      loopback,
                      alternateMultiStateObject,
                      BacnetPropertyId::NumberOfStates,
                      invokeId++,
                      value) &&
           value.type == BacnetValueType::Unsigned && value.unsignedValue == 2,
         "MSV2021 Number Of States mismatch");
  expect(readProperty(client,
                      server,
                      loopback,
                      multiStateObject,
                      BacnetPropertyId::StateText,
                      invokeId++,
                      value,
                      0) &&
           value.type == BacnetValueType::Unsigned && value.unsignedValue == 3,
         "MSV2020 State Text array count mismatch");
  expect(readProperty(client,
                      server,
                      loopback,
                      multiStateObject,
                      BacnetPropertyId::StateText,
                      invokeId++,
                      value,
                      2) &&
           value.type == BacnetValueType::CharacterString && std::strcmp(value.text, "Auto") == 0,
         "MSV2020 State Text array entry mismatch");
  expect(readProperty(client,
                      server,
                      loopback,
                      alternateMultiStateObject,
                      BacnetPropertyId::StateText,
                      invokeId++,
                      value,
                      2) &&
           value.type == BacnetValueType::CharacterString && std::strcmp(value.text, "Run") == 0,
         "MSV2021 State Text array entry mismatch");
  expect(readProperty(client,
                      server,
                      loopback,
                      multiStateObject,
                      BacnetPropertyId::ObjectName,
                      invokeId++,
                      value) &&
           value.type == BacnetValueType::CharacterString &&
           std::strcmp(value.text, kRegisteredMsvName) == 0,
         "registered MSV base property read failed");
  expect(readProperty(client,
                      server,
                      loopback,
                      multiStateObject,
                      BacnetPropertyId::Description,
                      invokeId++,
                      value) &&
           value.type == BacnetValueType::CharacterString &&
           std::strcmp(value.text, kMsvDescription) == 0,
         "registered MSV optional property read failed");
  expect(readProperty(client,
                      server,
                      loopback,
                      multiStateObject,
                      BacnetPropertyId::PropertyList,
                      invokeId++,
                      value,
                      0) &&
           value.type == BacnetValueType::Unsigned && value.unsignedValue == 11,
         "MSV2020 Property List count mismatch");
  const BacnetPropertyId msv2020Properties[] = {
    BacnetPropertyId::ObjectIdentifier,
    BacnetPropertyId::ObjectName,
    BacnetPropertyId::ObjectType,
    BacnetPropertyId::PresentValue,
    BacnetPropertyId::StatusFlags,
    BacnetPropertyId::EventState,
    BacnetPropertyId::OutOfService,
    BacnetPropertyId::NumberOfStates,
    BacnetPropertyId::StateText,
    BacnetPropertyId::PropertyList,
    BacnetPropertyId::Description,
  };
  for (size_t propertyIndex = 0; propertyIndex < sizeof(msv2020Properties) / sizeof(msv2020Properties[0]); ++propertyIndex) {
    const BacnetPropertyId property = msv2020Properties[propertyIndex];
    expect(readProperty(client,
                        server,
                        loopback,
                        multiStateObject,
                        BacnetPropertyId::PropertyList,
                        invokeId++,
                        value,
                        static_cast<uint32_t>(propertyIndex + 1U)) &&
             value.type == BacnetValueType::Enumerated &&
             value.unsignedValue == static_cast<uint32_t>(property),
           "MSV2020 Property List entry mismatch");
    const uint32_t arrayIndex = property == BacnetPropertyId::StateText ||
                                        property == BacnetPropertyId::PropertyList
                                      ? 0U
                                      : kBacnetNoArrayIndex;
    expect(readProperty(client, server, loopback, multiStateObject, property, invokeId++, value,
                        arrayIndex),
           "MSV2020 announced property read failed");
  }
  expect(readProperty(client,
                      server,
                      loopback,
                      alternateMultiStateObject,
                      BacnetPropertyId::PropertyList,
                      invokeId++,
                      value,
                      0) &&
           value.type == BacnetValueType::Unsigned && value.unsignedValue == 10,
         "MSV2021 Property List count mismatch");
  expect(readPropertyError(
           client,
           server,
           loopback,
           BacnetPropertyRequest{multiStateObject, BacnetPropertyId::StateText, 4},
           invokeId++,
           2,
           42),
         "MSV2020 invalid State Text index must return a BACnet error");

  multiStateValues[0].presentValue = 0;
  expect(readPropertyError(client,
                           server,
                           loopback,
                           BacnetPropertyRequest{multiStateObject,
                                                 BacnetPropertyId::PresentValue,
                                                 kBacnetNoArrayIndex},
                           invokeId++,
                           2,
                           37),
         "MSV2020 zero Present Value must return value-out-of-range");
  multiStateValues[0].presentValue = 4;
  expect(readPropertyError(client,
                           server,
                           loopback,
                           BacnetPropertyRequest{multiStateObject,
                                                 BacnetPropertyId::PresentValue,
                                                 kBacnetNoArrayIndex},
                           invokeId++,
                           2,
                           37),
         "MSV2020 out-of-range Present Value must return value-out-of-range");
  multiStateValues[0].presentValue = 2;
  multiStateValues[0].numberOfStates = 0;
  expect(readPropertyError(client,
                           server,
                           loopback,
                           BacnetPropertyRequest{multiStateObject,
                                                 BacnetPropertyId::NumberOfStates,
                                                 kBacnetNoArrayIndex},
                           invokeId++,
                           2,
                           37),
         "MSV2020 zero Number Of States must return value-out-of-range");
  multiStateValues[0].numberOfStates = 3;
  multiStateValues[0].stateText = nullptr;
  expect(readPropertyError(client,
                           server,
                           loopback,
                           BacnetPropertyRequest{multiStateObject,
                                                 BacnetPropertyId::StateText,
                                                 0},
                           invokeId++,
                           2,
                           37),
         "MSV2020 missing State Text must return value-out-of-range");
  multiStateValues[0].stateText = stateText;

  client.end();
  server.end();
  expect(!client.isRunning() && !server.isRunning(),
         "client and server must stop without background work") &&
    expect(!clientTransport.isOpen() && !serverTransport.isOpen(),
           "client and server UDP transports must close cleanly");
  runtime.end();
}

void testProtocolObjectTypesWithoutMultiStateValue() {
  WindowsSocketRuntime runtime;
  WindowsBacnetDatagramTransport serverTransport(runtime);
  WindowsBacnetDatagramTransport clientTransport(runtime);
  BacnetServer server(serverTransport);
  BacnetClient client(clientTransport);
  const BacnetIpEndpoint serverEndpoint(127, 0, 0, 1, 47810);
  const BacnetIpEndpoint clientEndpoint(127, 0, 0, 1, 47811);
  const BacnetServerDevice device{
    kDeviceInstance + 1U,
    kVendorId,
    "No MSV Loopback Test Device",
    "Unregistered BACnet Test Server",
    "Windows Test Harness",
    "0.39.0",
    "0.39.0",
  };
  const BacnetObjectId deviceObject{
    static_cast<uint16_t>(BacnetObjectType::Device), device.deviceInstance};
  if (!expect(runtime.begin(), "Winsock runtime without MSV failed") ||
      !expect(serverTransport.setBindAddress(serverEndpoint),
              "server loopback bind configuration without MSV failed") ||
      !expect(clientTransport.setBindAddress(clientEndpoint),
              "client loopback bind configuration without MSV failed") ||
      !expect(server.begin(device, serverEndpoint.port), "server loopback bind without MSV failed") ||
      !expect(client.begin(clientEndpoint.port), "client loopback bind without MSV failed")) {
    client.end();
    server.end();
    runtime.end();
    return;
  }

  BacnetValue value;
  expect(readProperty(client,
                      server,
                      serverEndpoint,
                      deviceObject,
                      BacnetPropertyId::ProtocolObjectTypesSupported,
                      220,
                      value) &&
           value.type == BacnetValueType::BitString && value.bitStringBitCount == 9 &&
           value.bitStringValue == (1UL << static_cast<uint16_t>(BacnetObjectType::Device)),
         "Device Protocol Object Types without MSV must keep the 9-bit bitmap");
  client.end();
  server.end();
  runtime.end();
}

} // namespace

int main() {
  testLoopbackServerClientPath();
  testProtocolObjectTypesWithoutMultiStateValue();
  return failures == 0 ? 0 : 1;
}
