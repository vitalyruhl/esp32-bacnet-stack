// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>

#include "portable/BacnetRuntime.h"

#ifndef BACNET_ENABLE_LOGGING
#define BACNET_ENABLE_LOGGING 1
#endif

#ifndef BACNET_ENABLE_VERBOSE_LOGGING
#define BACNET_ENABLE_VERBOSE_LOGGING 0
#endif

#ifndef BACNET_LOG_MAX_OUTPUTS
#define BACNET_LOG_MAX_OUTPUTS 4
#endif

#ifndef BACNET_LOG_MAX_TAG_DEPTH
#define BACNET_LOG_MAX_TAG_DEPTH 4
#endif

enum class BacnetLogLevel : uint8_t {
  Off = 0,
  Fatal = 1,
  Error = 2,
  Warn = 3,
  Info = 4,
  Debug = 5,
  Trace = 6,
};

enum class BacnetLogTimestampMode : uint8_t {
  None = 0,
  Millis = 1,
  DateTime = 2,
};

struct BacnetLogRecord {
  // tag and message are valid only during BacnetLogOutput::log().
  // Buffered outputs must copy both fields before returning from log().
  BacnetLogLevel level = BacnetLogLevel::Info;
  const char* tag = "BACnet";
  const char* message = "";
  uint32_t timestampMs = 0;
  const char* component = nullptr;
  const char* event = nullptr;
};

using BacnetLogFilter = bool (*)(const BacnetLogRecord& record,
                                 const void* userData);

class BacnetLogOutput {
public:
  virtual ~BacnetLogOutput() = default;

  virtual void log(const BacnetLogRecord& record) = 0;
  virtual void tick(uint32_t nowMs) {
    (void)nowMs;
  }

  void setLevel(BacnetLogLevel level) {
    level_ = level;
  }
  BacnetLogLevel level() const {
    return level_;
  }

  void setTimestampMode(BacnetLogTimestampMode mode) {
    timestampMode_ = mode;
  }
  BacnetLogTimestampMode timestampMode() const {
    return timestampMode_;
  }

  void setMinIntervalMs(uint32_t intervalMs) {
    minIntervalMs_ = intervalMs;
  }
  uint32_t minIntervalMs() const {
    return minIntervalMs_;
  }

  void setFilter(BacnetLogFilter filter, const void* userData = nullptr) {
    filter_ = filter;
    filterUserData_ = userData;
  }

  bool shouldLog(const BacnetLogRecord& record);

private:
  BacnetLogLevel level_ = BacnetLogLevel::Info;
  BacnetLogTimestampMode timestampMode_ = BacnetLogTimestampMode::None;
  uint32_t minIntervalMs_ = 0;
  uint32_t lastLogMs_ = 0;
  bool hasLastLogMs_ = false;
  BacnetLogFilter filter_ = nullptr;
  const void* filterUserData_ = nullptr;
};

class BacnetScopedLogTag;

class BacnetLogger {
public:
  static constexpr size_t kMaxMessageLength = 192;
  static constexpr size_t kMaxTagLength = 96;
  static constexpr size_t kMaxOutputs = BACNET_LOG_MAX_OUTPUTS;
  static constexpr size_t kMaxTagDepth = BACNET_LOG_MAX_TAG_DEPTH;

  explicit BacnetLogger(const BacnetMonotonicClock* clock = nullptr)
      : clock_(clock) {}

  void setClock(const BacnetMonotonicClock* clock) {
    clock_ = clock;
  }
  const BacnetMonotonicClock* clock() const {
    return clock_;
  }

  void addOutput(BacnetLogOutput& output);
  bool removeOutput(const BacnetLogOutput& output);
  void clearOutputs();
  size_t outputCount() const;

  void setGlobalLevel(BacnetLogLevel level) {
    globalLevel_ = level;
  }
  BacnetLogLevel globalLevel() const {
    return globalLevel_;
  }

  void setTag(const char* tag);
  void clearTag();
  void pushTag(const char* tag);
  void popTag();
  BacnetScopedLogTag scopedTag(const char* tag);

  void emit(const BacnetLogRecord& record);
  void tick(uint32_t nowMs);
  void tick();

  void logTag(BacnetLogLevel level, const char* tag, const char* format, ...);
  void log(BacnetLogLevel level, const char* format, ...);
  void fatal(const char* tag, const char* format, ...);
  void error(const char* tag, const char* format, ...);
  void warn(const char* tag, const char* format, ...);
  void info(const char* tag, const char* format, ...);
  void debug(const char* tag, const char* format, ...);
  void trace(const char* tag, const char* format, ...);

  static const char* levelName(BacnetLogLevel level);
  static const char* levelPrefix(BacnetLogLevel level);
  static bool isEnabledFor(BacnetLogLevel level);

private:
  void logV(BacnetLogLevel level, const char* tag, const char* format, va_list args);
  bool shouldEmit(BacnetLogLevel level) const;
  void buildTag(const char* explicitTag, char* buffer, size_t bufferSize) const;

  BacnetLogOutput*
    outputs_[(kMaxOutputs > 0) ? kMaxOutputs : 1] = {};
  size_t outputCount_ = 0;
  BacnetLogLevel globalLevel_ = BacnetLogLevel::Info;
  char baseTag_[kMaxTagLength] = {};
  char tagStack_[(kMaxTagDepth > 0) ? kMaxTagDepth : 1][kMaxTagLength] = {};
  size_t tagDepth_ = 0;
  const BacnetMonotonicClock* clock_ = nullptr;
};

class BacnetScopedLogTag {
public:
  BacnetScopedLogTag(BacnetLogger* logger, const char* tag) : logger_(logger) {
    if (logger_ != nullptr) {
      logger_->pushTag(tag);
    }
  }
  ~BacnetScopedLogTag() {
    if (logger_ != nullptr) {
      logger_->popTag();
    }
  }

  BacnetScopedLogTag(const BacnetScopedLogTag&) = delete;
  BacnetScopedLogTag& operator=(const BacnetScopedLogTag&) = delete;

  BacnetScopedLogTag(BacnetScopedLogTag&& other) noexcept
      : logger_(other.logger_) {
    other.logger_ = nullptr;
  }
  BacnetScopedLogTag& operator=(BacnetScopedLogTag&& other) noexcept {
    if (this != &other) {
      if (logger_ != nullptr) {
        logger_->popTag();
      }
      logger_ = other.logger_;
      other.logger_ = nullptr;
    }
    return *this;
  }

private:
  BacnetLogger* logger_ = nullptr;
};
