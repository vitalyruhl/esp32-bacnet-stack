// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetCliSupport.h"
#include "BacnetClient.h"
#include "platform/windows/WindowsBacnetDatagramTransport.h"
#include "platform/windows/WindowsMonotonicClock.h"
#include "platform/windows/WindowsSocketRuntime.h"

#include <chrono>
#include <cstdio>
#include <cstring>

namespace {
constexpr uint32_t kDefaultTimeoutMs = 5000;
constexpr uint32_t kMaximumTimeoutMs = 600000;
constexpr uint32_t kMaximumDeviceInstance = 0x3fffff;

struct DiscoveryOptions {
  BacnetIpEndpoint bindEndpoint;
  BacnetIpEndpoint broadcastEndpoint;
  uint32_t timeoutMs = kDefaultTimeoutMs;
  uint32_t deviceId = 0;
  bool hasBind = false;
  bool hasBroadcast = false;
  bool hasDeviceId = false;
};

void printHelp(FILE* output = stdout) {
  std::fputs("Usage:\n"
             "  bacnet-discover-smoke --help\n"
             "  bacnet-discover-smoke --self-test\n"
             "  bacnet-discover-smoke --bind <local-ip> --broadcast <broadcast-ip> [--timeout-ms <milliseconds>] [--device-id <id>]\n",
             output);
}

bool parseArguments(int argc, char* argv[], DiscoveryOptions& options) {
  for (int index = 1; index < argc; ++index) {
    const char* argument = argv[index];
    if (std::strcmp(argument, "--bind") == 0 && !options.hasBind && index + 1 < argc) {
      options.bindEndpoint.port = BacnetClient::kDefaultPort;
      options.hasBind = bacnetCliParseIpv4(argv[++index], options.bindEndpoint);
      if (!options.hasBind) {
        return false;
      }
    } else if (std::strcmp(argument, "--broadcast") == 0 && !options.hasBroadcast &&
               index + 1 < argc) {
      options.broadcastEndpoint.port = BacnetClient::kDefaultPort;
      options.hasBroadcast = bacnetCliParseIpv4(argv[++index], options.broadcastEndpoint);
      if (!options.hasBroadcast) {
        return false;
      }
    } else if (std::strcmp(argument, "--timeout-ms") == 0 && index + 1 < argc) {
      if (!bacnetCliParseUnsigned(argv[++index], options.timeoutMs) || options.timeoutMs == 0 ||
          options.timeoutMs > kMaximumTimeoutMs) {
        return false;
      }
    } else if (std::strcmp(argument, "--device-id") == 0 && !options.hasDeviceId &&
               index + 1 < argc) {
      options.hasDeviceId = bacnetCliParseUnsigned(argv[++index], options.deviceId) &&
                            options.deviceId <= kMaximumDeviceInstance;
      if (!options.hasDeviceId) {
        return false;
      }
    } else {
      return false;
    }
  }
  return options.hasBind && options.hasBroadcast;
}

int runSelfTest() {
  WindowsSocketRuntime runtime;
  if (!runtime.begin()) {
    std::fputs("[E] Winsock initialization failed\n", stderr);
    return static_cast<int>(BacnetCliExitCode::RuntimeFailure);
  }
  WindowsBacnetDatagramTransport transport(runtime);
  if (!transport.begin(0)) {
    std::fputs("[E] Windows UDP transport initialization failed\n", stderr);
    return static_cast<int>(BacnetCliExitCode::RuntimeFailure);
  }
  transport.end();
  runtime.end();
  return static_cast<int>(BacnetCliExitCode::Success);
}

void printDevice(const BacnetIAmDevice& device) {
  std::printf("device-id=%lu ip=%u.%u.%u.%u port=%u\n",
              static_cast<unsigned long>(device.deviceInstance),
              static_cast<unsigned>(device.endpoint.address[0]),
              static_cast<unsigned>(device.endpoint.address[1]),
              static_cast<unsigned>(device.endpoint.address[2]),
              static_cast<unsigned>(device.endpoint.address[3]),
              static_cast<unsigned>(device.endpoint.port));
}

int runDiscovery(const DiscoveryOptions& options) {
  WindowsSocketRuntime runtime;
  if (!runtime.begin()) {
    std::fputs("[E] Winsock initialization failed\n", stderr);
    return static_cast<int>(BacnetCliExitCode::RuntimeFailure);
  }

  WindowsBacnetDatagramTransport transport(runtime);
  if (!transport.setBindAddress(options.bindEndpoint)) {
    std::fputs("[E] Windows UDP bind configuration failed\n", stderr);
    return static_cast<int>(BacnetCliExitCode::RuntimeFailure);
  }

  WindowsMonotonicClock clock;
  BacnetClient client(transport, &clock);
  if (!client.begin(BacnetClient::kDefaultPort)) {
    std::fputs("[E] Windows UDP bind failed\n", stderr);
    return static_cast<int>(BacnetCliExitCode::RuntimeFailure);
  }
  if (!client.sendWhoIs(options.broadcastEndpoint)) {
    std::fputs("[E] Who-Is send failed\n", stderr);
    client.end();
    return static_cast<int>(BacnetCliExitCode::RuntimeFailure);
  }

  bool found = false;
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(options.timeoutMs);
  while (std::chrono::steady_clock::now() < deadline) {
    BacnetIAmDevice device;
    if (client.pollIAm(device)) {
      if (!options.hasDeviceId || device.deviceInstance == options.deviceId) {
        printDevice(device);
        found = true;
      }
      continue;
    }
    if (transport.status() != WindowsBacnetTransportStatus::None) {
      std::fputs("[E] Windows UDP receive failed\n", stderr);
      client.end();
      return static_cast<int>(BacnetCliExitCode::RuntimeFailure);
    }
    client.idle();
  }

  client.end();
  if (!found) {
    std::fputs("[I] discovery timeout without a matching I-Am\n", stderr);
    return static_cast<int>(BacnetCliExitCode::Timeout);
  }
  return static_cast<int>(BacnetCliExitCode::Success);
}
} // namespace

int main(int argc, char* argv[]) {
  if (argc == 2 && std::strcmp(argv[1], "--help") == 0) {
    printHelp();
    return static_cast<int>(BacnetCliExitCode::Success);
  }
  if (argc == 2 && std::strcmp(argv[1], "--self-test") == 0) {
    return runSelfTest();
  }

  DiscoveryOptions options;
  if (!parseArguments(argc, argv, options)) {
    std::fputs("[E] invalid command-line arguments\n", stderr);
    printHelp(stderr);
    return static_cast<int>(BacnetCliExitCode::InvalidArguments);
  }
  return runDiscovery(options);
}
