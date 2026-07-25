// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetDemoLogging.h"

#include <Arduino.h>

#include <cstdio>
#include <memory>

#ifndef CM_DISABLE_GUI_LOGGING
#define CM_DISABLE_GUI_LOGGING 0
#endif

BacnetDemoLogging* BacnetDemoLogging::active_ = nullptr;

BacnetDemoLogging::BacnetDemoLogging(ConfigManagerClass& configManager,
                                     BacnetClient& client)
    : configManager_(configManager), client_(client) {
  active_ = this;
}

void BacnetDemoLogging::setup() {
#if CLIENT_DEMO_ENABLE_LOGGING
#if !CM_DISABLE_GUI_LOGGING
  auto guiOut =
    std::make_unique<cm::LoggingManager::GuiOutput>(configManager_, 80);
  guiOut->setLevel(Level::Info);
  guiOut->setMaxQueue(120);
  guiOut->setMaxPerTick(8);
  guiOut->addTimestamp(cm::LoggingManager::Output::TimestampMode::Millis);
  cm::LoggingManager::instance().addOutput(std::move(guiOut));
#endif
  cm::LoggingManager::instance().setGlobalLevel(Level::Info);
  output_.setLevel(BacnetLogLevel::Info);
  output_.setTimestampMode(BacnetLogTimestampMode::Millis);
  output_.setMinIntervalMs(0);
  client_.logger().addOutput(output_);
#endif
}

void BacnetDemoLogging::loop() {
#if CLIENT_DEMO_ENABLE_LOGGING
  cm::LoggingManager::instance().loop();
#endif
}

void BacnetDemoLogging::log(Level level, const char* format, ...) {
  va_list args;
  va_start(args, format);
  logV(level, format, args);
  va_end(args);
}

void BacnetDemoLogging::logV(Level level, const char* format, va_list args) {
#if CLIENT_DEMO_ENABLE_LOGGING
  char message[160] = {};
  std::vsnprintf(message, sizeof(message), format, args);
  cm::LoggingManager::instance().logTag(level, "BACnet", "%s", message);
#else
  (void)level;
  (void)format;
  (void)args;
#endif
}

BacnetDemoLogCallback BacnetDemoLogging::watchedAnalogCallback() const {
  return logWatchedAnalog;
}

#if CLIENT_DEMO_ENABLE_LOGGING
void BacnetDemoLogging::Output::log(const BacnetLogRecord& record) {
  Serial.print(BacnetLogger::levelPrefix(record.level));
  Serial.print(" ");
  Serial.print(record.tag != nullptr ? record.tag : "BACnet");
  Serial.print(" ");
  Serial.println(record.message != nullptr ? record.message : "");
  cm::LoggingManager::instance().logTag(
    BacnetDemoLogging::toConfigManagerLevel(record.level),
    record.tag != nullptr ? record.tag : "BACnet",
    "%s",
    record.message != nullptr ? record.message : "");
}
#endif

void BacnetDemoLogging::logWatchedAnalog(BacnetDemoLogLevel level,
                                         const char* message) {
  if (active_ == nullptr) {
    return;
  }
  active_->log(level == BacnetDemoLogLevel::Warn ? Level::Warn : Level::Info,
               "%s",
               message != nullptr ? message : "");
}

BacnetDemoLogging::Level BacnetDemoLogging::toConfigManagerLevel(
  BacnetLogLevel level) {
  switch (level) {
    case BacnetLogLevel::Fatal:
      return Level::Fatal;
    case BacnetLogLevel::Error:
      return Level::Error;
    case BacnetLogLevel::Warn:
      return Level::Warn;
    case BacnetLogLevel::Debug:
      return Level::Debug;
    case BacnetLogLevel::Trace:
      return Level::Trace;
    case BacnetLogLevel::Info:
    case BacnetLogLevel::Off:
    default:
      return Level::Info;
  }
}
