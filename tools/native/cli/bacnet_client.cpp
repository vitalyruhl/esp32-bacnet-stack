// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetNativeCli.h"
#include "BacnetRemoteObject.h"
#include "portable/BacnetDisplayText.h"
#include "platform/windows/WindowsBacnetDatagramTransport.h"
#include "platform/windows/WindowsMonotonicClock.h"
#include "platform/windows/WindowsSocketRuntime.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#define NOMINMAX
#include <windows.h>

namespace {

enum class Command {
  None,
  List,
  Read,
  Subscribe,
  PrioritySlot,
  WriteBvPriority,
  RelinquishBvPriority,
};

struct CommandOptions {
  BacnetObjectSelector selector;
  BacnetPropertyId property = BacnetPropertyId::ObjectName;
  uint32_t maximum = 20;
  uint32_t lifetimeSeconds = 60;
  uint32_t durationSeconds = 0;
  uint8_t priority = 0;
  bool execute = false;
  bool hasPriority = false;
};

volatile bool gStopRequested = false;

BOOL WINAPI onConsoleControl(DWORD controlType) {
  if (controlType == CTRL_C_EVENT || controlType == CTRL_BREAK_EVENT) {
    gStopRequested = true;
    return TRUE;
  }
  return FALSE;
}

void printHelp(FILE* output = stdout) {
  std::fputs(
    "Usage:\n"
    "  bacnet-client --bind <local-ip> --device-id <id> [--broadcast <broadcast-ip>|--target <ip[:port]>] [--timeout-ms <milliseconds>] list [--max <count>] <object-selector>\n"
    "  bacnet-client --bind <local-ip> --device-id <id> [--broadcast <broadcast-ip>|--target <ip[:port]>] [--timeout-ms <milliseconds>] read [--verbose] <object.property>\n"
    "  bacnet-client --bind <local-ip> --device-id <id> --target <ip[:port]> [--timeout-ms <milliseconds>] subscribe <AV-instance> [--lifetime-seconds <seconds>] [--duration-seconds <seconds>]\n"
    "  bacnet-client --bind <local-ip> --device-id <id> --target <ip[:port]> [--timeout-ms <milliseconds>] priority-slot <BV-instance> --priority <1..16>\n"
    "  bacnet-client --bind <local-ip> --device-id <id> --target <ip[:port]> [--timeout-ms <milliseconds>] write-bv-priority <BV-instance> <active|inactive> --priority <1..16> --execute\n"
    "  bacnet-client --bind <local-ip> --device-id <id> --target <ip[:port]> [--timeout-ms <milliseconds>] relinquish-bv-priority <BV-instance> --priority <1..16> --execute\n"
    "\nObject selectors use AV, BV, and MSV prefixes (for example AV200). Priority writes require compile-time WriteProperty and priority-write gates. A confirmed priority write remains active until relinquished.\n"
    "Exit codes: 0 success, 1 runtime/error, 2 timeout, 3 invalid arguments, 4 reject, 5 abort, 6 decode error, 7 feature disabled, 8 missing --execute.\n",
    output);
}

bool parsePriority(const char* text, CommandOptions& commandOptions) {
  uint32_t priority = 0;
  if (!bacnetCliParseUnsigned(text, priority) || priority == 0 || priority > 16) {
    return false;
  }
  commandOptions.priority = static_cast<uint8_t>(priority);
  commandOptions.hasPriority = true;
  return true;
}

bool parseArguments(int argc,
                    char* argv[],
                    BacnetNativeCliOptions& options,
                    Command& command,
                    CommandOptions& commandOptions,
                    const char*& valueText) {
  const char* selectorText = nullptr;
  valueText = nullptr;
  for (int index = 1; index < argc; ++index) {
    const char* argument = argv[index];
    if (std::strcmp(argument, "list") == 0 && command == Command::None) command = Command::List;
    else if (std::strcmp(argument, "read") == 0 && command == Command::None) command = Command::Read;
    else if (std::strcmp(argument, "subscribe") == 0 && command == Command::None) command = Command::Subscribe;
    else if (std::strcmp(argument, "priority-slot") == 0 && command == Command::None) command = Command::PrioritySlot;
    else if (std::strcmp(argument, "write-bv-priority") == 0 && command == Command::None) command = Command::WriteBvPriority;
    else if (std::strcmp(argument, "relinquish-bv-priority") == 0 && command == Command::None) command = Command::RelinquishBvPriority;
    else if (std::strcmp(argument, "--bind") == 0 && !options.hasBind && index + 1 < argc) options.hasBind = bacnetCliParseIpv4(argv[++index], options.bindEndpoint);
    else if (std::strcmp(argument, "--broadcast") == 0 && !options.hasBroadcast && index + 1 < argc) options.hasBroadcast = bacnetCliParseIpv4(argv[++index], options.broadcastEndpoint);
    else if (std::strcmp(argument, "--target") == 0 && !options.hasTarget && index + 1 < argc) options.hasTarget = bacnetNativeParseEndpoint(argv[++index], options.targetEndpoint);
    else if ((std::strcmp(argument, "--device-id") == 0 || std::strcmp(argument, "-id") == 0) && !options.hasDeviceId && index + 1 < argc) options.hasDeviceId = bacnetCliParseUnsigned(argv[++index], options.deviceId) && options.deviceId <= 0x3fffff;
    else if (std::strcmp(argument, "--timeout-ms") == 0 && index + 1 < argc) { if (!bacnetCliParseUnsigned(argv[++index], options.timeoutMs) || options.timeoutMs == 0) return false; }
    else if (std::strcmp(argument, "--max") == 0 && index + 1 < argc) { if (!bacnetCliParseUnsigned(argv[++index], commandOptions.maximum)) return false; }
    else if (std::strcmp(argument, "--priority") == 0 && index + 1 < argc) { if (!parsePriority(argv[++index], commandOptions)) return false; }
    else if (std::strcmp(argument, "--lifetime-seconds") == 0 && index + 1 < argc) { if (!bacnetCliParseUnsigned(argv[++index], commandOptions.lifetimeSeconds) || commandOptions.lifetimeSeconds == 0) return false; }
    else if (std::strcmp(argument, "--duration-seconds") == 0 && index + 1 < argc) { if (!bacnetCliParseUnsigned(argv[++index], commandOptions.durationSeconds) || commandOptions.durationSeconds == 0) return false; }
    else if (std::strcmp(argument, "--execute") == 0) commandOptions.execute = true;
    else if (std::strcmp(argument, "--verbose") == 0) options.verbose = true;
    else if (argument[0] != '-' && selectorText == nullptr) selectorText = argument;
    else if (argument[0] != '-' && valueText == nullptr && command == Command::WriteBvPriority) valueText = argument;
    else return false;
  }

  options.bindEndpoint.port = BacnetClient::kDefaultPort;
  options.broadcastEndpoint.port = BacnetClient::kDefaultPort;
  const bool hasEndpoint = options.hasBroadcast || options.hasTarget;
  if (!options.hasBind || !options.hasDeviceId || !hasEndpoint || selectorText == nullptr) return false;
  if (command == Command::Read) return bacnetNativeParseObjectPropertySelector(selectorText, commandOptions.selector, commandOptions.property);
  if (!bacnetNativeParseObjectSelector(selectorText, commandOptions.selector)) return false;
  const BacnetObjectType type = static_cast<BacnetObjectType>(commandOptions.selector.object.type);
  if (command == Command::List) return true;
  if (command == Command::Subscribe) return type == BacnetObjectType::AnalogValue;
  if (command == Command::PrioritySlot || command == Command::RelinquishBvPriority) return type == BacnetObjectType::BinaryValue && commandOptions.hasPriority;
  if (command == Command::WriteBvPriority) return type == BacnetObjectType::BinaryValue && commandOptions.hasPriority && valueText != nullptr && (std::strcmp(valueText, "active") == 0 || std::strcmp(valueText, "inactive") == 0);
  return false;
}

int resultExitCode(BacnetNativeReadStatus status) {
  return static_cast<int>(bacnetNativeReadExitCode(status));
}

int writeExitCode(BacnetDeviceSessionWriteStatus status) {
  switch (status) {
    case BacnetDeviceSessionWriteStatus::Ack: return static_cast<int>(BacnetCliExitCode::Success);
    case BacnetDeviceSessionWriteStatus::Timeout: return static_cast<int>(BacnetCliExitCode::Timeout);
    case BacnetDeviceSessionWriteStatus::Reject: return static_cast<int>(BacnetCliExitCode::Reject);
    case BacnetDeviceSessionWriteStatus::Abort: return static_cast<int>(BacnetCliExitCode::Abort);
    case BacnetDeviceSessionWriteStatus::Disabled: return static_cast<int>(BacnetCliExitCode::FeatureDisabled);
    case BacnetDeviceSessionWriteStatus::InvalidArgument: return static_cast<int>(BacnetCliExitCode::InvalidArguments);
    default: return static_cast<int>(BacnetCliExitCode::RuntimeFailure);
  }
}

int runRead(BacnetClient& client, const BacnetIpEndpoint& endpoint, const BacnetNativeCliOptions& options, const CommandOptions& commandOptions) {
  BacnetValue value;
  const BacnetNativeReadStatus status = bacnetNativeReadProperty(client, endpoint, BacnetPropertyRequest{commandOptions.selector.object, commandOptions.property}, options.timeoutMs, value);
  if (status != BacnetNativeReadStatus::Ack) { std::fprintf(stderr, "[E] read %s failed: %s\n", bacnetPropertyName(commandOptions.property), bacnetNativeReadStatusText(status)); return resultExitCode(status); }
  char text[BacnetValue::kMaxTextLength] = {};
  if (options.verbose) { char object[24] = {}; std::printf("object=%s\nproperty=%s\nvalue=", bacnetNativeObjectToken(commandOptions.selector.object, object, sizeof(object)), bacnetPropertyName(commandOptions.property)); }
  if (commandOptions.selector.object.type == static_cast<uint16_t>(BacnetObjectType::BinaryValue) &&
      commandOptions.property == BacnetPropertyId::PresentValue &&
      value.type == BacnetValueType::Enumerated) {
    if (value.unsignedValue == 0) { std::puts("inactive"); return 0; }
    if (value.unsignedValue == 1) { std::puts("active"); return 0; }
  }
  std::puts(bacnetValueDisplayText(value, text, sizeof(text)));
  return 0;
}

int runList(BacnetDeviceSession& session,
            const CommandOptions& commandOptions,
            uint32_t timeoutMs) {
  constexpr size_t kMaxScanResults = 600;
  BacnetScannedObject scanned[kMaxScanResults];
  const BacnetObjectType objectTypes[] = {
    static_cast<BacnetObjectType>(commandOptions.selector.object.type)};
  BacnetObjectScanOptions scanOptions;
  scanOptions.maxObjectListEntries = kMaxScanResults;
  scanOptions.readTimeoutMs = timeoutMs;
  scanOptions.readObjectName = true;
  scanOptions.readDescription = true;
  scanOptions.readPresentValue = false;
  bacnetSetObjectTypeFilter(scanOptions, objectTypes);

  const BacnetObjectScanResult summary =
    session.scanObjectList(scanOptions, scanned, kMaxScanResults);
  if (summary.objectListCountStatus != BacnetDeviceSessionReadStatus::Ack) {
    std::fprintf(stderr, "[E] object-list failed: %s\n",
                 bacnetReadStatusText(summary.objectListCountStatus));
    return static_cast<int>(BacnetCliExitCode::RuntimeFailure);
  }

  std::vector<const BacnetScannedObject*> matches;
  for (size_t index = 0; index < summary.stored; ++index) {
    if (scanned[index].objectId.instance >= commandOptions.selector.object.instance) {
      matches.push_back(&scanned[index]);
    }
  }
  std::sort(matches.begin(), matches.end(),
            [](const BacnetScannedObject* left, const BacnetScannedObject* right) {
              return left->objectId.instance < right->objectId.instance;
            });

  size_t errors = 0;
  const size_t listed = std::min(matches.size(), static_cast<size_t>(commandOptions.maximum));
  for (size_t index = 0; index < listed; ++index) {
    const BacnetScannedObject& entry = *matches[index];
    if (entry.objectNameStatus != BacnetDeviceSessionReadStatus::Ack) ++errors;
    if (entry.descriptionStatus != BacnetDeviceSessionReadStatus::Ack) ++errors;
    char token[24] = {};
    char nameText[BacnetValue::kMaxTextLength] = {};
    char descriptionText[BacnetValue::kMaxTextLength] = {};
    std::printf("%s - Name: %s, Description: %s\n",
                bacnetNativeObjectToken(entry.objectId, token, sizeof(token)),
                entry.objectNameStatus == BacnetDeviceSessionReadStatus::Ack
                  ? bacnetValueDisplayText(entry.objectName, nameText, sizeof(nameText))
                  : (std::string("<") + bacnetReadStatusText(entry.objectNameStatus) + ">").c_str(),
                entry.descriptionStatus == BacnetDeviceSessionReadStatus::Ack
                  ? bacnetValueDisplayText(entry.description, descriptionText, sizeof(descriptionText))
                  : (std::string("<") + bacnetReadStatusText(entry.descriptionStatus) + ">").c_str());
  }
  std::fprintf(stderr, "Listed: %zu\nMatched: %zu\nSkipped: %zu\nProperty errors: %zu\n",
               listed, matches.size(), matches.size() - listed, errors);
  return 0;
}

int runPrioritySlot(BacnetDeviceSession& session, const CommandOptions& commandOptions, uint32_t timeoutMs) {
  BacnetValue value;
  const BacnetPropertyReadStatus status = session.object(commandOptions.selector.object).readPriorityArray(value, timeoutMs, commandOptions.priority);
  if (status != BacnetPropertyReadStatus::Ack && status != BacnetPropertyReadStatus::EmptyValue) { std::fprintf(stderr, "[E] priority slot read failed: %s\n", bacnetPropertyReadStatusText(status)); return static_cast<int>(BacnetCliExitCode::RuntimeFailure); }
  char text[BacnetValue::kMaxTextLength] = {};
  std::printf("priority=%u\nvalue=%s\n", static_cast<unsigned>(commandOptions.priority), bacnetValueDisplayText(value, text, sizeof(text)));
  return 0;
}

int runWrite(BacnetDeviceSession& session, const CommandOptions& commandOptions, const char* valueText, uint32_t timeoutMs, bool relinquish) {
  if (!commandOptions.execute) {
    std::fputs("[E] write commands require --execute\n", stderr);
    return static_cast<int>(BacnetCliExitCode::WriteNotAuthorized);
  }
  BacnetRemoteObject object = session.object(commandOptions.selector.object);
  BacnetDeviceSessionWriteStatus status;
  if (relinquish) {
    status = object.relinquishPresentValue(commandOptions.priority, timeoutMs);
  } else {
    BacnetValue value;
    value.type = BacnetValueType::Enumerated;
    value.unsignedValue = std::strcmp(valueText, "active") == 0 ? 1U : 0U;
    status = object.writePresentValue(value, commandOptions.priority, timeoutMs);
  }
  if (status != BacnetDeviceSessionWriteStatus::Ack) {
    std::fprintf(stderr, "[E] %s failed: %s\n", relinquish ? "relinquish" : "priority write", bacnetWriteStatusText(status));
    return writeExitCode(status);
  }
  std::printf("%s BV%lu priority %u acknowledged\n", relinquish ? "relinquish" : "write", static_cast<unsigned long>(commandOptions.selector.object.instance), static_cast<unsigned>(commandOptions.priority));
  return 0;
}

const char* covStatusText(BacnetCovSubscriptionStatus status) {
  switch (status) {
    case BacnetCovSubscriptionStatus::Pending: return "pending";
    case BacnetCovSubscriptionStatus::Active: return "active";
    case BacnetCovSubscriptionStatus::Error: return "error";
    case BacnetCovSubscriptionStatus::Reject: return "reject";
    case BacnetCovSubscriptionStatus::Abort: return "abort";
    case BacnetCovSubscriptionStatus::Timeout: return "timeout";
    case BacnetCovSubscriptionStatus::SendFailed: return "send-failed";
  }
  return "unknown";
}

int covExitCode(BacnetCovSubscriptionStatus status) {
  if (status == BacnetCovSubscriptionStatus::Timeout) return static_cast<int>(BacnetCliExitCode::Timeout);
  if (status == BacnetCovSubscriptionStatus::Reject) return static_cast<int>(BacnetCliExitCode::Reject);
  if (status == BacnetCovSubscriptionStatus::Abort) return static_cast<int>(BacnetCliExitCode::Abort);
  return static_cast<int>(BacnetCliExitCode::RuntimeFailure);
}

void printCovFailure(const char* phase, const BacnetPropertySubscription& subscription) {
  if (subscription.covStatus() == BacnetCovSubscriptionStatus::Reject) {
    const uint8_t code = subscription.covRejectReason();
    std::fprintf(stderr, "[E] SubscribeCOV %s: reject reason=%s code=%u; polling fallback is disabled\n", phase, BacnetProtocol::rejectReasonText(code), static_cast<unsigned>(code));
    return;
  }
  std::fprintf(stderr, "[E] SubscribeCOV %s: %s; polling fallback is disabled\n", phase, covStatusText(subscription.covStatus()));
}

void printCovNotification(const BacnetSubscriptionNotification& notification) {
  if (!notification.hasValue || !notification.valueChanged) return;
  std::time_t now = std::time(nullptr);
  std::tm local = {};
  localtime_s(&local, &now);
  char timestamp[32] = {};
  std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &local);
  char object[24] = {}; char value[BacnetValue::kMaxTextLength] = {};
  std::printf("%s  %s Present Value = %s\n", timestamp, bacnetNativeObjectToken(notification.objectId, object, sizeof(object)), bacnetValueDisplayText(*notification.value, value, sizeof(value)));
  std::fflush(stdout);
}

int runSubscribe(BacnetDeviceSession& session, BacnetClient& client, const CommandOptions& commandOptions, uint32_t timeoutMs) {
  BacnetSubscribeOptions options;
  options.preferCov = true;
  options.fallbackPollMs = 0;
  options.immediateFirstRead = false;
  options.issueConfirmedNotifications = false;
  options.timeoutMs = timeoutMs;
  options.covLifetimeSeconds = commandOptions.lifetimeSeconds;
  options.covRenewBeforeSeconds = commandOptions.lifetimeSeconds > 5 ? 5 : 1;
  BacnetPropertySubscription subscription = session.object(commandOptions.selector.object).property(BacnetPropertyId::PresentValue).subscribe(printCovNotification, nullptr, options);
  const uint32_t startedAt = client.nowMs();
  while (subscription.covStatus() == BacnetCovSubscriptionStatus::Pending && client.nowMs() - startedAt < timeoutMs) {
    session.poll(subscription);
    client.idle();
  }
  if (subscription.covStatus() != BacnetCovSubscriptionStatus::Active) {
    printCovFailure("failed", subscription);
    subscription.stop();
    return covExitCode(subscription.covStatus());
  }
  char object[24] = {};
  std::printf("SubscribeCOV active for %s. Press Ctrl+C to stop.\n", bacnetNativeObjectToken(commandOptions.selector.object, object, sizeof(object)));
  SetConsoleCtrlHandler(onConsoleControl, TRUE);
  const uint32_t activeAt = client.nowMs();
  while (!gStopRequested) {
    if (commandOptions.durationSeconds != 0 &&
        client.nowMs() - activeAt >= commandOptions.durationSeconds * 1000UL) {
      break;
    }
    session.poll(subscription);
    if (subscription.covStatus() != BacnetCovSubscriptionStatus::Active) {
      printCovFailure("renewal failed", subscription);
      SetConsoleCtrlHandler(onConsoleControl, FALSE);
      subscription.stop();
      return covExitCode(subscription.covStatus());
    }
    client.idle();
  }
  SetConsoleCtrlHandler(onConsoleControl, FALSE);
  subscription.stop();
  std::puts("SubscribeCOV stopped locally.");
  return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc == 2 && std::strcmp(argv[1], "--help") == 0) { printHelp(); return 0; }
  if (argc == 3 && std::strcmp(argv[2], "--help") == 0) { printHelp(); return 0; }
  BacnetNativeCliOptions options;
  Command command = Command::None;
  CommandOptions commandOptions;
  const char* valueText = nullptr;
  if (!parseArguments(argc, argv, options, command, commandOptions, valueText)) { std::fputs("[E] invalid command-line arguments\n", stderr); printHelp(stderr); return static_cast<int>(BacnetCliExitCode::InvalidArguments); }
  if ((command == Command::WriteBvPriority || command == Command::RelinquishBvPriority) && !commandOptions.execute) return static_cast<int>(BacnetCliExitCode::WriteNotAuthorized);
  WindowsSocketRuntime runtime;
  WindowsBacnetDatagramTransport transport(runtime);
  WindowsMonotonicClock clock;
  if (!runtime.begin() || !transport.setBindAddress(options.bindEndpoint)) { std::fputs("[E] Windows UDP initialization failed\n", stderr); return static_cast<int>(BacnetCliExitCode::RuntimeFailure); }
  BacnetClient client(transport, &clock);
  if (!client.begin(0)) { std::fputs("[E] Windows UDP bind failed\n", stderr); return static_cast<int>(BacnetCliExitCode::RuntimeFailure); }
  BacnetIpEndpoint endpoint;
  if (!bacnetNativeResolveDevice(client, transport, options, endpoint)) { std::fputs("[E] device resolution timeout or failure\n", stderr); client.end(); return static_cast<int>(BacnetCliExitCode::Timeout); }
  int result = 0;
  if (command == Command::Read) result = runRead(client, endpoint, options, commandOptions);
  else {
    BacnetDeviceSession session(client, options.deviceId, endpoint);
    if (command == Command::List) result = runList(session, commandOptions, options.timeoutMs);
    else if (command == Command::Subscribe) result = runSubscribe(session, client, commandOptions, options.timeoutMs);
    else if (command == Command::PrioritySlot) result = runPrioritySlot(session, commandOptions, options.timeoutMs);
    else result = runWrite(session, commandOptions, valueText, options.timeoutMs, command == Command::RelinquishBvPriority);
  }
  client.end();
  return result;
}
