// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetCliSupport.h"
#include "BacnetNativeCli.h"
#include "BacnetWriteHil.h"
#include "WindowsInterfaceSelection.h"
#include "portable/BacnetDisplayText.h"
#include "platform/windows/WindowsBacnetDatagramTransport.h"
#include "platform/windows/WindowsMonotonicClock.h"
#include "platform/windows/WindowsSocketRuntime.h"

#include <cstdio>
#include <cstring>

namespace {

struct Options {
  BacnetIpEndpoint bind;
  BacnetIpEndpoint broadcast;
  BacnetIpEndpoint target;
  uint32_t deviceId = 0;
  uint32_t avInstance = 0;
  uint32_t bvInstance = 0;
  uint32_t msvInstance = 0;
  uint32_t timeoutMs = 3000;
  uint8_t priority = 8;
  bool hasBind = false;
  bool hasBroadcast = false;
  bool hasTarget = false;
  bool hasDeviceId = false;
  bool hasAv = false;
  bool hasBv = false;
  bool hasMsv = false;
};

void printHelp(FILE* output = stdout) {
  std::fputs(
    "Usage: bacnet-write-hil --bind <local-ip> --broadcast <broadcast-ip> "
    "--device-id <id> --target <ip:port> --av <instance> --bv <instance> "
    "--msv <instance> [--priority 8] [--timeout-ms <milliseconds>]\n",
    output);
}

bool parseEndpoint(const char* text, BacnetIpEndpoint& endpoint) {
  return bacnetNativeParseEndpoint(text, endpoint);
}

bool parseArguments(int argc, char* argv[], Options& options) {
  for (int index = 1; index < argc; ++index) {
    const char* argument = argv[index];
    if (std::strcmp(argument, "--bind") == 0 && !options.hasBind &&
        index + 1 < argc) {
      options.hasBind = bacnetCliParseIpv4(argv[++index], options.bind);
    } else if (std::strcmp(argument, "--broadcast") == 0 &&
               !options.hasBroadcast && index + 1 < argc) {
      options.hasBroadcast = bacnetCliParseIpv4(argv[++index], options.broadcast);
    } else if (std::strcmp(argument, "--target") == 0 && !options.hasTarget &&
               index + 1 < argc) {
      options.hasTarget = parseEndpoint(argv[++index], options.target);
    } else if (std::strcmp(argument, "--device-id") == 0 &&
               !options.hasDeviceId && index + 1 < argc) {
      options.hasDeviceId = bacnetCliParseUnsigned(argv[++index], options.deviceId) &&
                            options.deviceId <= 0x3fffff;
    } else if (std::strcmp(argument, "--av") == 0 && !options.hasAv &&
               index + 1 < argc) {
      options.hasAv = bacnetCliParseUnsigned(argv[++index], options.avInstance) &&
                      options.avInstance != 0;
    } else if (std::strcmp(argument, "--bv") == 0 && !options.hasBv &&
               index + 1 < argc) {
      options.hasBv = bacnetCliParseUnsigned(argv[++index], options.bvInstance) &&
                      options.bvInstance != 0;
    } else if (std::strcmp(argument, "--msv") == 0 && !options.hasMsv &&
               index + 1 < argc) {
      options.hasMsv = bacnetCliParseUnsigned(argv[++index], options.msvInstance) &&
                       options.msvInstance != 0;
    } else if (std::strcmp(argument, "--priority") == 0 && index + 1 < argc) {
      uint32_t priority = 0;
      if (!bacnetCliParseUnsigned(argv[++index], priority) || priority == 0 ||
          priority > 16) {
        return false;
      }
      options.priority = static_cast<uint8_t>(priority);
    } else if (std::strcmp(argument, "--timeout-ms") == 0 && index + 1 < argc) {
      if (!bacnetCliParseUnsigned(argv[++index], options.timeoutMs) ||
          options.timeoutMs == 0) {
        return false;
      }
    } else {
      return false;
    }
  }
  options.bind.port = BacnetClient::kDefaultPort;
  options.broadcast.port = BacnetClient::kDefaultPort;
  return options.hasBind && options.hasBroadcast && options.hasTarget &&
         options.hasDeviceId && options.hasAv && options.hasBv && options.hasMsv;
}

void printResult(const char* label, const BacnetWriteHilResult& result) {
  std::printf("[HIL] %s stage=%s write=%u relinquish=%u reset-completed=%u "
              "reset-skipped=%u reset-failed=%u cleanup=%s cleanup-status=%u "
              "priority-may-be-active=%s\n",
              label, bacnetWriteHilStageText(result.stage),
              static_cast<unsigned>(result.writeStatus),
              static_cast<unsigned>(result.relinquishStatus),
              static_cast<unsigned>(result.reset.completedPriorities),
              static_cast<unsigned>(result.reset.skippedPriority),
              static_cast<unsigned>(result.reset.failedPriority),
              result.cleanupAttempted ? "attempted" : "not-needed",
              static_cast<unsigned>(result.cleanupStatus),
              result.priorityMayBeActive ? "yes" : "no");
  char original[BacnetValue::kMaxTextLength] = {};
  char temporary[BacnetValue::kMaxTextLength] = {};
  char slotEight[BacnetValue::kMaxTextLength] = {};
  char slotSixteen[BacnetValue::kMaxTextLength] = {};
  std::printf("[HIL] %s original=%s temporary=%s slot8=%s slot16=%s\n", label,
              bacnetValueDisplayText(result.originalPresentValue, original, sizeof(original)),
              bacnetValueDisplayText(result.temporaryValue, temporary, sizeof(temporary)),
              bacnetValueDisplayText(result.slotEight, slotEight, sizeof(slotEight)),
              bacnetValueDisplayText(result.slotSixteen, slotSixteen, sizeof(slotSixteen)));
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc == 2 && std::strcmp(argv[1], "--help") == 0) {
    printHelp();
    return 0;
  }
  Options options;
  if (!parseArguments(argc, argv, options)) {
    std::fputs("[E] invalid command-line arguments\n", stderr);
    printHelp(stderr);
    return static_cast<int>(BacnetCliExitCode::InvalidArguments);
  }

  WindowsIpv4Interface selected;
  WindowsIpv4Interface candidates[8];
  size_t candidateCount = 0;
  if (windowsDiscoverIpv4Interface(&options.bind, selected, candidates,
                                   sizeof(candidates) / sizeof(candidates[0]),
                                   candidateCount) !=
      WindowsInterfaceSelectionStatus::Selected) {
    std::fputs("[E] requested Windows IPv4 interface was not selected\n", stderr);
    return static_cast<int>(BacnetCliExitCode::RuntimeFailure);
  }
  selected.address.port = BacnetClient::kDefaultPort;

  WindowsSocketRuntime runtime;
  WindowsBacnetDatagramTransport transport(runtime);
  WindowsMonotonicClock clock;
  if (!runtime.begin() || !transport.setBindAddress(selected.address)) {
    std::fputs("[E] Windows UDP initialization failed\n", stderr);
    return static_cast<int>(BacnetCliExitCode::RuntimeFailure);
  }
  BacnetClient client(transport, &clock);
  if (!client.begin()) {
    std::fputs("[E] Windows UDP bind failed\n", stderr);
    return static_cast<int>(BacnetCliExitCode::RuntimeFailure);
  }

  BacnetDeviceSession session(client, options.deviceId, options.target);
  const BacnetWriteHilOptions runOptions{options.priority, options.timeoutMs};
  const BacnetWriteHilTarget targets[] = {
    {BacnetObjectType::AnalogValue, options.avInstance, true},
    {BacnetObjectType::BinaryValue, options.bvInstance, false},
    {BacnetObjectType::MultiStateValue, options.msvInstance, false},
  };
  const char* labels[] = {"AV", "BV", "MSV"};
  for (size_t index = 0; index < sizeof(targets) / sizeof(targets[0]); ++index) {
    const BacnetWriteHilResult result =
      bacnetWriteHilRunTarget(session, targets[index], runOptions);
    printResult(labels[index], result);
    if (!result.succeeded()) {
      client.end();
      return static_cast<int>(BacnetCliExitCode::RuntimeFailure);
    }
  }
  client.end();
  std::puts("[PASS] Windows priority WriteProperty HIL complete");
  return 0;
}
