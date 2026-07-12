// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include "BacnetLogger.h"

class WindowsConsoleLogSink final : public BacnetLogOutput {
public:
  void log(const BacnetLogRecord& record) override;
};
