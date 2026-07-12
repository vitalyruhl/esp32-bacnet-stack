// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetNativeCli.h"
#include "platform/windows/WindowsBacnetDatagramTransport.h"
#include "platform/windows/WindowsMonotonicClock.h"
#include "platform/windows/WindowsSocketRuntime.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
enum class Command { None, List, Read };

void printHelp(FILE* output = stdout) {
  std::fputs("Usage:\n  bacnet-client --bind <local-ip> --device-id <id> [--broadcast <broadcast-ip>|--target <ip[:port]>] [--timeout-ms <milliseconds>] list [--max <count>] <object-selector>\n  bacnet-client --bind <local-ip> --device-id <id> [--broadcast <broadcast-ip>|--target <ip[:port]>] [--timeout-ms <milliseconds>] read [--verbose] <object.property>\n", output);
}

bool parseArguments(int argc, char* argv[], BacnetNativeCliOptions& options, Command& command, BacnetObjectSelector& selector, BacnetPropertyId& property, uint32_t& maximum) {
  const char* selectorText = nullptr;
  maximum = 20;
  for (int index = 1; index < argc; ++index) {
    const char* argument = argv[index];
    if (std::strcmp(argument, "list") == 0 && command == Command::None) command = Command::List;
    else if (std::strcmp(argument, "read") == 0 && command == Command::None) command = Command::Read;
    else if (std::strcmp(argument, "--bind") == 0 && !options.hasBind && index + 1 < argc) options.hasBind = bacnetCliParseIpv4(argv[++index], options.bindEndpoint);
    else if (std::strcmp(argument, "--broadcast") == 0 && !options.hasBroadcast && index + 1 < argc) options.hasBroadcast = bacnetCliParseIpv4(argv[++index], options.broadcastEndpoint);
    else if (std::strcmp(argument, "--target") == 0 && !options.hasTarget && index + 1 < argc) options.hasTarget = bacnetNativeParseEndpoint(argv[++index], options.targetEndpoint);
    else if ((std::strcmp(argument, "--device-id") == 0 || std::strcmp(argument, "-id") == 0) && !options.hasDeviceId && index + 1 < argc) options.hasDeviceId = bacnetCliParseUnsigned(argv[++index], options.deviceId) && options.deviceId <= 0x3fffff;
    else if (std::strcmp(argument, "--timeout-ms") == 0 && index + 1 < argc) { if (!bacnetCliParseUnsigned(argv[++index], options.timeoutMs) || options.timeoutMs == 0) return false; }
    else if (std::strcmp(argument, "--max") == 0 && index + 1 < argc) { if (!bacnetCliParseUnsigned(argv[++index], maximum)) return false; }
    else if (std::strcmp(argument, "--verbose") == 0) options.verbose = true;
    else if (argument[0] != '-' && selectorText == nullptr) selectorText = argument;
    else return false;
  }
  options.bindEndpoint.port = BacnetClient::kDefaultPort;
  options.broadcastEndpoint.port = BacnetClient::kDefaultPort;
  if (command == Command::List) return options.hasBind && options.hasDeviceId && (options.hasBroadcast || options.hasTarget) && selectorText != nullptr && bacnetNativeParseObjectSelector(selectorText, selector);
  if (command == Command::Read) return options.hasBind && options.hasDeviceId && (options.hasBroadcast || options.hasTarget) && selectorText != nullptr && bacnetNativeParseObjectPropertySelector(selectorText, selector, property);
  return false;
}

int resultExitCode(BacnetNativeReadStatus status) { return static_cast<int>(bacnetNativeReadExitCode(status)); }

int runRead(BacnetClient& client, const BacnetIpEndpoint& endpoint, const BacnetNativeCliOptions& options, BacnetObjectSelector selector, BacnetPropertyId property) {
  BacnetValue value;
  const BacnetNativeReadStatus status = bacnetNativeReadProperty(client, endpoint, BacnetPropertyRequest{selector.object, property}, options.timeoutMs, value);
  if (status != BacnetNativeReadStatus::Ack) { std::fprintf(stderr, "[E] read %s failed: %s\n", bacnetPropertyName(property), bacnetNativeReadStatusText(status)); return resultExitCode(status); }
  if (options.verbose) { char object[24] = {}; std::printf("object=%s\nproperty=%s\nvalue=", bacnetNativeObjectToken(selector.object, object, sizeof(object)), bacnetPropertyName(property)); }
  std::puts(value.displayText());
  return 0;
}

int runList(BacnetClient& client, const BacnetIpEndpoint& endpoint, const BacnetNativeCliOptions& options, BacnetObjectSelector selector, uint32_t maximum) {
  BacnetValue value;
  const BacnetObjectId device{static_cast<uint16_t>(BacnetObjectType::Device), options.deviceId};
  BacnetNativeReadStatus status = bacnetNativeReadProperty(client, endpoint, BacnetPropertyRequest{device, BacnetPropertyId::ObjectList, 0}, options.timeoutMs, value);
  if (status != BacnetNativeReadStatus::Ack || value.type != BacnetValueType::Unsigned) { std::fprintf(stderr, "[E] object-list failed: %s\n", status == BacnetNativeReadStatus::Ack ? "decode-error" : bacnetNativeReadStatusText(status)); return status == BacnetNativeReadStatus::Ack ? 1 : resultExitCode(status); }
  std::vector<BacnetObjectId> matches;
  for (uint32_t index = 1; index <= value.unsignedValue; ++index) {
    BacnetValue entry;
    status = bacnetNativeReadProperty(client, endpoint, BacnetPropertyRequest{device, BacnetPropertyId::ObjectList, index}, options.timeoutMs, entry);
    if (status == BacnetNativeReadStatus::Ack && entry.type == BacnetValueType::ObjectIdentifier && entry.objectValue.type == selector.object.type && entry.objectValue.instance >= selector.object.instance) matches.push_back(entry.objectValue);
  }
  std::sort(matches.begin(), matches.end(), [](const BacnetObjectId& left, const BacnetObjectId& right) { return left.instance < right.instance; });
  size_t errors = 0; const size_t listed = std::min(matches.size(), static_cast<size_t>(maximum));
  for (size_t index = 0; index < listed; ++index) {
    BacnetValue name, description;
    const BacnetNativeReadStatus nameStatus = bacnetNativeReadProperty(client, endpoint, BacnetPropertyRequest{matches[index], BacnetPropertyId::ObjectName}, options.timeoutMs, name);
    const BacnetNativeReadStatus descriptionStatus = bacnetNativeReadProperty(client, endpoint, BacnetPropertyRequest{matches[index], BacnetPropertyId::Description}, options.timeoutMs, description);
    if (nameStatus != BacnetNativeReadStatus::Ack) ++errors;
    if (descriptionStatus != BacnetNativeReadStatus::Ack) ++errors;
    char token[24] = {};
    std::printf("%s - Name: %s, Description: %s\n", bacnetNativeObjectToken(matches[index], token, sizeof(token)), nameStatus == BacnetNativeReadStatus::Ack ? name.displayText() : (std::string("<") + bacnetNativeReadStatusText(nameStatus) + ">").c_str(), descriptionStatus == BacnetNativeReadStatus::Ack ? description.displayText() : (std::string("<") + bacnetNativeReadStatusText(descriptionStatus) + ">").c_str());
  }
  std::fprintf(stderr, "Listed: %zu\nMatched: %zu\nProperty errors: %zu\n", listed, matches.size(), errors);
  return 0;
}
} // namespace

int main(int argc, char* argv[]) {
  if (argc == 2 && std::strcmp(argv[1], "--help") == 0) { printHelp(); return 0; }
  if (argc == 3 && (std::strcmp(argv[1], "list") == 0 || std::strcmp(argv[1], "read") == 0) && std::strcmp(argv[2], "--help") == 0) { printHelp(); return 0; }
  BacnetNativeCliOptions options; Command command = Command::None; BacnetObjectSelector selector; BacnetPropertyId property = BacnetPropertyId::ObjectName; uint32_t maximum = 0;
  if (!parseArguments(argc, argv, options, command, selector, property, maximum)) { std::fputs("[E] invalid command-line arguments\n", stderr); printHelp(stderr); return 3; }
  WindowsSocketRuntime runtime; WindowsBacnetDatagramTransport transport(runtime); WindowsMonotonicClock clock;
  if (!runtime.begin() || !transport.setBindAddress(options.bindEndpoint)) { std::fputs("[E] Windows UDP initialization failed\n", stderr); return 1; }
  BacnetClient client(transport, &clock);
  if (!client.begin()) { std::fputs("[E] Windows UDP bind failed\n", stderr); return 1; }
  BacnetIpEndpoint endpoint;
  if (!bacnetNativeResolveDevice(client, transport, options, endpoint)) { std::fputs("[E] device resolution timeout or failure\n", stderr); client.end(); return 2; }
  const int result = command == Command::List ? runList(client, endpoint, options, selector, maximum) : runRead(client, endpoint, options, selector, property);
  client.end();
  return result;
}
