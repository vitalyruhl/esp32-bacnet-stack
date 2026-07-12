// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "platform/windows/WindowsConsoleLogSink.h"

#include <cstdio>

void WindowsConsoleLogSink::log(const BacnetLogRecord& record) {
  std::fprintf(stderr, "%s %s: %s\n", BacnetLogger::levelPrefix(record.level), record.tag != nullptr ? record.tag : "BACnet", record.message != nullptr ? record.message : "");
}
