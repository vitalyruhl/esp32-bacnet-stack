// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetCliSupport.h"
#include "platform/windows/WindowsBacnetDatagramTransport.h"
#include "platform/windows/WindowsConsoleLogSink.h"
#include "platform/windows/WindowsMonotonicClock.h"
#include "platform/windows/WindowsSocketRuntime.h"

#include <cstdio>
#include <cstring>

namespace {
void printHelp(FILE* output = stdout) {
  std::fputs("Usage: bacnet_cli_smoke [--help|--version|--self-test]\n", output);
}

int runSelfTest() {
  WindowsSocketRuntime runtime;
  if (!runtime.begin()) {
    std::fputs("[E] Winsock runtime initialization failed\n", stderr);
    return static_cast<int>(BacnetCliExitCode::RuntimeFailure);
  }

  WindowsBacnetDatagramTransport transport(runtime);
  if (!transport.begin(0)) {
    std::fputs("[E] Windows UDP transport initialization failed\n", stderr);
    return static_cast<int>(BacnetCliExitCode::RuntimeFailure);
  }

  WindowsMonotonicClock clock;
  WindowsConsoleLogSink sink;
  sink.log(BacnetLogRecord{BacnetLogLevel::Debug, "BACnet/Smoke", "self-test ready", clock.nowMs(), "BACnet", nullptr});
  transport.end();
  runtime.end();
  return static_cast<int>(BacnetCliExitCode::Success);
}
} // namespace

int main(int argc, char* argv[]) {
  if (argc == 1 || std::strcmp(argv[1], "--help") == 0) {
    printHelp();
    return static_cast<int>(BacnetCliExitCode::Success);
  }
  if (std::strcmp(argv[1], "--version") == 0) {
    std::puts("bacnet_cli_smoke 0.28.0");
    return static_cast<int>(BacnetCliExitCode::Success);
  }
  if (std::strcmp(argv[1], "--self-test") == 0) {
    return runSelfTest();
  }

  std::fprintf(stderr, "[E] %s: %s\n", bacnetCliExitCodeText(BacnetCliExitCode::InvalidArguments), argv[1]);
  printHelp(stderr);
  return static_cast<int>(BacnetCliExitCode::InvalidArguments);
}
