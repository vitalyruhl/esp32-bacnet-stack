// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetNativeCli.h"

#include "platform/windows/WindowsBacnetDatagramTransport.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace {
constexpr uint32_t kMaximumDeviceInstance = 0x3fffff;

bool equalsIgnoreCase(const char* left, const char* right) {
  if (left == nullptr || right == nullptr) return false;
  while (*left != '\0' && *right != '\0') {
    if (std::tolower(static_cast<unsigned char>(*left)) !=
        std::tolower(static_cast<unsigned char>(*right))) return false;
    ++left;
    ++right;
  }
  return *left == '\0' && *right == '\0';
}

bool parseObjectType(const char* text, size_t length, BacnetObjectType& type) {
  if (length == 2 && (std::toupper(text[0]) == 'A') && std::toupper(text[1]) == 'I') { type = BacnetObjectType::AnalogInput; return true; }
  if (length == 2 && (std::toupper(text[0]) == 'A') && std::toupper(text[1]) == 'O') { type = BacnetObjectType::AnalogOutput; return true; }
  if (length == 2 && (std::toupper(text[0]) == 'A') && std::toupper(text[1]) == 'V') { type = BacnetObjectType::AnalogValue; return true; }
  if (length == 2 && (std::toupper(text[0]) == 'B') && std::toupper(text[1]) == 'I') { type = BacnetObjectType::BinaryInput; return true; }
  if (length == 2 && (std::toupper(text[0]) == 'B') && std::toupper(text[1]) == 'O') { type = BacnetObjectType::BinaryOutput; return true; }
  if (length == 2 && (std::toupper(text[0]) == 'B') && std::toupper(text[1]) == 'V') { type = BacnetObjectType::BinaryValue; return true; }
  if (length == 3 && std::toupper(text[0]) == 'M' && std::toupper(text[1]) == 'S' && std::toupper(text[2]) == 'I') { type = BacnetObjectType::MultiStateInput; return true; }
  if (length == 3 && std::toupper(text[0]) == 'M' && std::toupper(text[1]) == 'S' && std::toupper(text[2]) == 'O') { type = BacnetObjectType::MultiStateOutput; return true; }
  if (length == 3 && std::toupper(text[0]) == 'M' && std::toupper(text[1]) == 'S' && std::toupper(text[2]) == 'V') { type = BacnetObjectType::MultiStateValue; return true; }
  if (length == 6 && std::toupper(text[0]) == 'D' && std::toupper(text[1]) == 'E' && std::toupper(text[2]) == 'V' && std::toupper(text[3]) == 'I' && std::toupper(text[4]) == 'C' && std::toupper(text[5]) == 'E') { type = BacnetObjectType::Device; return true; }
  return false;
}

bool parseEndpointPort(const char* text, const char*& colon) {
  colon = std::strrchr(text, ':');
  return colon != nullptr && colon != text && colon[1] != '\0';
}
} // namespace

bool bacnetNativeParseEndpoint(const char* text, BacnetIpEndpoint& endpoint) {
  if (text == nullptr) return false;
  const char* colon = nullptr;
  if (!parseEndpointPort(text, colon)) return bacnetCliParseIpv4(text, endpoint);
  char address[16] = {};
  const size_t addressLength = static_cast<size_t>(colon - text);
  if (addressLength >= sizeof(address)) return false;
  std::memcpy(address, text, addressLength);
  uint32_t port = 0;
  if (!bacnetCliParseIpv4(address, endpoint) || !bacnetCliParseUnsigned(colon + 1, port) || port == 0 || port > UINT16_MAX) return false;
  endpoint.port = static_cast<uint16_t>(port);
  return true;
}

bool bacnetNativeParseObjectSelector(const char* text, BacnetObjectSelector& selector) {
  if (text == nullptr || text[0] == '\0') return false;
  size_t prefixLength = 0;
  while (std::isalpha(static_cast<unsigned char>(text[prefixLength]))) ++prefixLength;
  if (prefixLength == 0 || text[prefixLength] == '\0') return false;
  BacnetObjectType type;
  uint32_t instance = 0;
  const char* instanceText = text + prefixLength;
  if (*instanceText == ',') {
    ++instanceText;
  }
  if (!parseObjectType(text, prefixLength, type) ||
      !bacnetCliParseUnsigned(instanceText, instance) ||
      instance > kMaximumDeviceInstance) return false;
  selector.object = BacnetObjectId{static_cast<uint16_t>(type), instance};
  return true;
}

bool bacnetNativeParseProperty(const char* text, BacnetPropertyId& property) {
  struct Property { const char* name; BacnetPropertyId id; };
  static constexpr Property properties[] = {
    {"object-identifier", BacnetPropertyId::ObjectIdentifier}, {"objectIdentifier", BacnetPropertyId::ObjectIdentifier},
    {"object-name", BacnetPropertyId::ObjectName}, {"objectName", BacnetPropertyId::ObjectName},
    {"description", BacnetPropertyId::Description}, {"present-value", BacnetPropertyId::PresentValue}, {"presentValue", BacnetPropertyId::PresentValue},
    {"status-flags", BacnetPropertyId::StatusFlags}, {"statusFlags", BacnetPropertyId::StatusFlags}, {"event-state", BacnetPropertyId::EventState},
    {"reliability", BacnetPropertyId::Reliability}, {"out-of-service", BacnetPropertyId::OutOfService}, {"outOfService", BacnetPropertyId::OutOfService},
    {"priority-array", BacnetPropertyId::PriorityArray}, {"priorityArray", BacnetPropertyId::PriorityArray},
    {"units", BacnetPropertyId::Units},
    {"vendor-name", BacnetPropertyId::VendorName}, {"vendorName", BacnetPropertyId::VendorName},
    {"vendor-id", BacnetPropertyId::VendorIdentifier}, {"vendorIdentifier", BacnetPropertyId::VendorIdentifier},
    {"model-name", BacnetPropertyId::ModelName}, {"modelName", BacnetPropertyId::ModelName},
    {"firmware-revision", BacnetPropertyId::FirmwareRevision}, {"firmwareRevision", BacnetPropertyId::FirmwareRevision},
    {"application-software-version", BacnetPropertyId::ApplicationSoftwareVersion}, {"applicationSoftwareVersion", BacnetPropertyId::ApplicationSoftwareVersion},
    {"object-type", BacnetPropertyId::ObjectType}, {"objectType", BacnetPropertyId::ObjectType},
    {"protocol-version", BacnetPropertyId::ProtocolVersion}, {"protocolVersion", BacnetPropertyId::ProtocolVersion},
    {"protocol-revision", BacnetPropertyId::ProtocolRevision}, {"protocolRevision", BacnetPropertyId::ProtocolRevision},
    {"object-list", BacnetPropertyId::ObjectList}, {"objectList", BacnetPropertyId::ObjectList},
    {"property-list", BacnetPropertyId::PropertyList}, {"propertyList", BacnetPropertyId::PropertyList}};
  for (const Property& candidate : properties) if (equalsIgnoreCase(text, candidate.name)) { property = candidate.id; return true; }
  return false;
}

bool bacnetNativeParseObjectPropertySelector(const char* text, BacnetObjectSelector& selector, BacnetPropertyId& property) {
  uint32_t arrayIndex = kBacnetNoArrayIndex;
  return bacnetNativeParseObjectPropertySelector(text, selector, property, arrayIndex);
}

bool bacnetNativeParseObjectPropertySelector(const char* text,
                                             BacnetObjectSelector& selector,
                                             BacnetPropertyId& property,
                                             uint32_t& arrayIndex) {
  if (text == nullptr) return false;
  const char* separator = std::strchr(text, '.');
  if (separator == nullptr || separator == text || separator[1] == '\0') return false;
  char object[24] = {};
  const size_t length = static_cast<size_t>(separator - text);
  if (length >= sizeof(object)) return false;
  std::memcpy(object, text, length);
  const char* propertyText = separator + 1;
  const char* bracket = std::strchr(propertyText, '[');
  arrayIndex = kBacnetNoArrayIndex;
  if (bracket == nullptr) {
    return bacnetNativeParseObjectSelector(object, selector) &&
           bacnetNativeParseProperty(propertyText, property);
  }

  const char* closing = std::strchr(bracket + 1, ']');
  if (closing == nullptr || closing[1] != '\0') return false;
  char propertyName[48] = {};
  const size_t propertyLength = static_cast<size_t>(bracket - propertyText);
  if (propertyLength == 0 || propertyLength >= sizeof(propertyName)) return false;
  std::memcpy(propertyName, propertyText, propertyLength);
  uint32_t parsedIndex = 0;
  char indexText[16] = {};
  const size_t indexLength = static_cast<size_t>(closing - bracket - 1);
  if (indexLength == 0 || indexLength >= sizeof(indexText)) return false;
  std::memcpy(indexText, bracket + 1, indexLength);
  if (!bacnetNativeParseObjectSelector(object, selector) ||
      !bacnetNativeParseProperty(propertyName, property) ||
      !bacnetCliParseUnsigned(indexText, parsedIndex)) return false;
  arrayIndex = parsedIndex;
  return true;
}

const char* bacnetNativeObjectToken(BacnetObjectId object, char* buffer, size_t capacity) {
  const char* prefix = "OBJECT";
  switch (static_cast<BacnetObjectType>(object.type)) {
    case BacnetObjectType::AnalogInput: prefix = "AI"; break; case BacnetObjectType::AnalogOutput: prefix = "AO"; break; case BacnetObjectType::AnalogValue: prefix = "AV"; break;
    case BacnetObjectType::BinaryInput: prefix = "BI"; break; case BacnetObjectType::BinaryOutput: prefix = "BO"; break; case BacnetObjectType::BinaryValue: prefix = "BV"; break;
    case BacnetObjectType::MultiStateInput: prefix = "MSI"; break; case BacnetObjectType::MultiStateOutput: prefix = "MSO"; break; case BacnetObjectType::MultiStateValue: prefix = "MSV"; break;
    case BacnetObjectType::Device: prefix = "DEVICE"; break;
  }
  if (buffer == nullptr || capacity == 0 || std::snprintf(buffer, capacity, "%s%lu", prefix, static_cast<unsigned long>(object.instance)) <= 0) return nullptr;
  return buffer;
}

const char* bacnetNativeReadStatusText(BacnetNativeReadStatus status) {
  switch (status) { case BacnetNativeReadStatus::Ack: return "ok"; case BacnetNativeReadStatus::Error: return "error"; case BacnetNativeReadStatus::Reject: return "reject"; case BacnetNativeReadStatus::Abort: return "abort"; case BacnetNativeReadStatus::Timeout: return "timeout"; case BacnetNativeReadStatus::DecodeError: return "decode-error"; case BacnetNativeReadStatus::SendFailed: return "send-failed"; } return "runtime failure";
}

BacnetCliExitCode bacnetNativeReadExitCode(BacnetNativeReadStatus status) {
  switch (status) { case BacnetNativeReadStatus::Ack: return BacnetCliExitCode::Success; case BacnetNativeReadStatus::Timeout: return BacnetCliExitCode::Timeout; case BacnetNativeReadStatus::Reject: return BacnetCliExitCode::Reject; case BacnetNativeReadStatus::Abort: return BacnetCliExitCode::Abort; case BacnetNativeReadStatus::DecodeError: return BacnetCliExitCode::DecodeError; default: return BacnetCliExitCode::RuntimeFailure; }
}

bool bacnetNativeResolveDevice(BacnetClient& client, WindowsBacnetDatagramTransport& transport, const BacnetNativeCliOptions& options, BacnetIpEndpoint& endpoint) {
  if (options.hasTarget) { endpoint = options.targetEndpoint; return true; }
  if (!options.hasDeviceId || !options.hasBroadcast) return false;
  if (!client.sendWhoIs(options.broadcastEndpoint)) return false;
  const uint32_t startedAt = client.nowMs();
  while (client.nowMs() - startedAt < options.timeoutMs) {
    BacnetIAmDevice device;
    if (client.pollIAm(device) && device.deviceInstance == options.deviceId) { endpoint = device.endpoint; return true; }
    if (transport.status() != WindowsBacnetTransportStatus::None) return false;
    client.idle();
  }
  return false;
}

BacnetNativeReadStatus bacnetNativeReadProperty(BacnetClient& client, const BacnetIpEndpoint& endpoint, BacnetPropertyRequest request, uint32_t timeoutMs, BacnetValue& value) {
  constexpr uint8_t kInvokeId = 1;
  if (!client.sendReadProperty(endpoint, request, kInvokeId)) return BacnetNativeReadStatus::SendFailed;
  const uint32_t startedAt = client.nowMs();
  while (client.nowMs() - startedAt < timeoutMs) {
    switch (client.pollReadPropertyStatus(value, kInvokeId, request)) {
      case BacnetReadPropertyPollStatus::Ack: return BacnetNativeReadStatus::Ack;
      case BacnetReadPropertyPollStatus::Error: return BacnetNativeReadStatus::Error;
      case BacnetReadPropertyPollStatus::Reject: return BacnetNativeReadStatus::Reject;
      case BacnetReadPropertyPollStatus::Abort: return BacnetNativeReadStatus::Abort;
      case BacnetReadPropertyPollStatus::DecodeError: return BacnetNativeReadStatus::DecodeError;
      case BacnetReadPropertyPollStatus::None: client.idle(); break;
    }
  }
  return BacnetNativeReadStatus::Timeout;
}
