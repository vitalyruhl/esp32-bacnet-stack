// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetNativeCli.h"
#include "platform/windows/WindowsBacnetDatagramTransport.h"
#include "platform/windows/WindowsMonotonicClock.h"
#include "platform/windows/WindowsSocketRuntime.h"

#include <cstdio>
#include <cstring>

namespace {
void printHelp(FILE* output = stdout) {
  std::fputs("Usage: bacnet-discover --bind <local-ip> --broadcast <broadcast-ip> [--device-id <id>] [--timeout-ms <milliseconds>]\n", output);
}

bool parseArguments(int argc, char* argv[], BacnetNativeCliOptions& options) {
  for (int index = 1; index < argc; ++index) {
    const char* argument = argv[index];
    if (std::strcmp(argument, "--bind") == 0 && !options.hasBind && index + 1 < argc) { options.hasBind = bacnetCliParseIpv4(argv[++index], options.bindEndpoint); }
    else if (std::strcmp(argument, "--broadcast") == 0 && !options.hasBroadcast && index + 1 < argc) { options.hasBroadcast = bacnetCliParseIpv4(argv[++index], options.broadcastEndpoint); }
    else if (std::strcmp(argument, "--device-id") == 0 && !options.hasDeviceId && index + 1 < argc) { options.hasDeviceId = bacnetCliParseUnsigned(argv[++index], options.deviceId) && options.deviceId <= 0x3fffff; }
    else if (std::strcmp(argument, "--timeout-ms") == 0 && index + 1 < argc) { if (!bacnetCliParseUnsigned(argv[++index], options.timeoutMs) || options.timeoutMs == 0) return false; }
    else return false;
    if ((!options.hasBind && std::strcmp(argument, "--bind") == 0) || (!options.hasBroadcast && std::strcmp(argument, "--broadcast") == 0) || (!options.hasDeviceId && std::strcmp(argument, "--device-id") == 0)) return false;
  }
  options.bindEndpoint.port = BacnetClient::kDefaultPort;
  options.broadcastEndpoint.port = BacnetClient::kDefaultPort;
  return options.hasBind && options.hasBroadcast;
}

const char* metadataValue(BacnetClient& client, const BacnetIpEndpoint& endpoint, uint32_t deviceId, BacnetPropertyId property, uint32_t timeoutMs, char* buffer, size_t capacity) {
  BacnetValue value;
  const BacnetNativeReadStatus status = bacnetNativeReadProperty(client, endpoint, BacnetPropertyRequest{BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::Device), deviceId}, property}, timeoutMs, value);
  if (status == BacnetNativeReadStatus::Ack) { std::snprintf(buffer, capacity, "%s", value.displayText()); }
  else { std::snprintf(buffer, capacity, "<%s>", bacnetNativeReadStatusText(status)); }
  return buffer;
}

void printMetadata(BacnetClient& client, const BacnetIpEndpoint& endpoint, uint32_t deviceId, uint32_t timeoutMs) {
  char value[512] = {};
  std::printf("object-name=\"%s\"\n", metadataValue(client, endpoint, deviceId, BacnetPropertyId::ObjectName, timeoutMs, value, sizeof(value)));
  std::printf("vendor-name=\"%s\"\n", metadataValue(client, endpoint, deviceId, BacnetPropertyId::VendorName, timeoutMs, value, sizeof(value)));
  std::printf("vendor-id=%s\n", metadataValue(client, endpoint, deviceId, BacnetPropertyId::VendorIdentifier, timeoutMs, value, sizeof(value)));
  std::printf("model-name=\"%s\"\n", metadataValue(client, endpoint, deviceId, BacnetPropertyId::ModelName, timeoutMs, value, sizeof(value)));
  std::printf("firmware-revision=\"%s\"\n", metadataValue(client, endpoint, deviceId, BacnetPropertyId::FirmwareRevision, timeoutMs, value, sizeof(value)));
  std::printf("application-software-version=\"%s\"\n", metadataValue(client, endpoint, deviceId, BacnetPropertyId::ApplicationSoftwareVersion, timeoutMs, value, sizeof(value)));
  std::printf("protocol-version=%s\n", metadataValue(client, endpoint, deviceId, BacnetPropertyId::ProtocolVersion, timeoutMs, value, sizeof(value)));
  std::printf("protocol-revision=%s\n", metadataValue(client, endpoint, deviceId, BacnetPropertyId::ProtocolRevision, timeoutMs, value, sizeof(value)));
}

void printObjectCounts(BacnetClient& client, const BacnetIpEndpoint& endpoint, uint32_t deviceId, uint32_t timeoutMs) {
  uint32_t counts[20] = {};
  BacnetValue value;
  if (bacnetNativeReadProperty(client, endpoint, BacnetPropertyRequest{BacnetObjectId{8, deviceId}, BacnetPropertyId::ObjectList, 0}, timeoutMs, value) != BacnetNativeReadStatus::Ack || value.type != BacnetValueType::Unsigned) { std::puts("objects=<unsupported>"); return; }
  const uint32_t objectCount = value.unsignedValue;
  for (uint32_t index = 1; index <= objectCount; ++index) {
    if (bacnetNativeReadProperty(client, endpoint, BacnetPropertyRequest{BacnetObjectId{8, deviceId}, BacnetPropertyId::ObjectList, index}, timeoutMs, value) == BacnetNativeReadStatus::Ack && value.type == BacnetValueType::ObjectIdentifier && value.objectValue.type < 20) ++counts[value.objectValue.type];
  }
  std::printf("objects=\"AI:%lu AO:%lu AV:%lu BI:%lu BO:%lu BV:%lu MSI:%lu MSO:%lu MSV:%lu\"\n", static_cast<unsigned long>(counts[0]), static_cast<unsigned long>(counts[1]), static_cast<unsigned long>(counts[2]), static_cast<unsigned long>(counts[3]), static_cast<unsigned long>(counts[4]), static_cast<unsigned long>(counts[5]), static_cast<unsigned long>(counts[13]), static_cast<unsigned long>(counts[14]), static_cast<unsigned long>(counts[19]));
}
} // namespace

int main(int argc, char* argv[]) {
  if (argc == 2 && std::strcmp(argv[1], "--help") == 0) { printHelp(); return 0; }
  BacnetNativeCliOptions options;
  if (!parseArguments(argc, argv, options)) { std::fputs("[E] invalid command-line arguments\n", stderr); printHelp(stderr); return static_cast<int>(BacnetCliExitCode::InvalidArguments); }
  WindowsSocketRuntime runtime;
  WindowsBacnetDatagramTransport transport(runtime);
  WindowsMonotonicClock clock;
  if (!runtime.begin() || !transport.setBindAddress(options.bindEndpoint)) { std::fputs("[E] Windows UDP initialization failed\n", stderr); return 1; }
  BacnetClient client(transport, &clock);
  if (!client.begin()) { std::fputs("[E] Windows UDP bind failed\n", stderr); return 1; }
  if (!client.sendWhoIs(options.broadcastEndpoint)) { std::fputs("[E] Who-Is send failed\n", stderr); return 1; }
  const uint32_t start = client.nowMs(); bool found = false;
  while (client.nowMs() - start < options.timeoutMs) {
    BacnetIAmDevice device;
    if (client.pollIAm(device) && (!options.hasDeviceId || device.deviceInstance == options.deviceId)) {
      std::printf("device-id=%lu\nip=%u.%u.%u.%u\nport=%u\n", static_cast<unsigned long>(device.deviceInstance), static_cast<unsigned>(device.endpoint.address[0]), static_cast<unsigned>(device.endpoint.address[1]), static_cast<unsigned>(device.endpoint.address[2]), static_cast<unsigned>(device.endpoint.address[3]), static_cast<unsigned>(device.endpoint.port));
      printMetadata(client, device.endpoint, device.deviceInstance, options.timeoutMs);
      printObjectCounts(client, device.endpoint, device.deviceInstance, options.timeoutMs); found = true; break;
    }
    client.idle();
  }
  client.end();
  if (!found) { std::fputs("[I] discovery timeout without a matching I-Am\n", stderr); return 2; }
  return 0;
}
