// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <BacnetClient.h>

#include "ConfigManager.h"
#include "BacnetDemoWatchedAnalogValue.h"
#include "logging/LoggingManager.h"

#include <cstdarg>

#ifndef CLIENT_DEMO_ENABLE_LOGGING
#define CLIENT_DEMO_ENABLE_LOGGING 1
#endif

class BacnetDemoLogging {
public:
  using Level = cm::LoggingManager::Level;

  BacnetDemoLogging(ConfigManagerClass& configManager, BacnetClient& client);

  void setup();
  void loop();
  void log(Level level, const char* format, ...);
  void logV(Level level, const char* format, va_list args);

  BacnetDemoLogCallback watchedAnalogCallback() const;

private:
#if CLIENT_DEMO_ENABLE_LOGGING
  class Output : public BacnetLogOutput {
  public:
    void log(const BacnetLogRecord& record) override;
  };
#endif

  static BacnetDemoLogging* active_;

  static void logWatchedAnalog(BacnetDemoLogLevel level, const char* message);
  static Level toConfigManagerLevel(BacnetLogLevel level);

  ConfigManagerClass& configManager_;
  BacnetClient& client_;
#if CLIENT_DEMO_ENABLE_LOGGING
  Output output_;
#endif
};
