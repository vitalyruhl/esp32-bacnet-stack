// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetLogger.h"

#include <cstdio>
#include <cstring>

namespace {
bool isVerboseLevel(BacnetLogLevel level) {
  return level == BacnetLogLevel::Debug || level == BacnetLogLevel::Trace;
}

bool isLevelAllowed(BacnetLogLevel level, BacnetLogLevel threshold) {
  if (level == BacnetLogLevel::Off || threshold == BacnetLogLevel::Off) {
    return false;
  }
  return static_cast<uint8_t>(level) <= static_cast<uint8_t>(threshold);
}

void copyText(char* target, size_t targetSize, const char* source) {
  if (target == nullptr || targetSize == 0) {
    return;
  }

  target[0] = '\0';
  if (source == nullptr) {
    return;
  }

  const size_t sourceLength = std::strlen(source);
  const size_t copyLength =
    sourceLength < (targetSize - 1) ? sourceLength : (targetSize - 1);
  std::memcpy(target, source, copyLength);
  target[copyLength] = '\0';
}
} // namespace

bool BacnetLogOutput::shouldLog(const BacnetLogRecord& record) {
  if (!isLevelAllowed(record.level, level_)) {
    return false;
  }

  if (filter_ != nullptr && !filter_(record, filterUserData_)) {
    return false;
  }

  if (minIntervalMs_ != 0) {
    if (hasLastLogMs_ && record.timestampMs - lastLogMs_ < minIntervalMs_) {
      return false;
    }
    lastLogMs_ = record.timestampMs;
    hasLastLogMs_ = true;
  }

  return true;
}

void BacnetLogger::addOutput(BacnetLogOutput& output) {
  for (size_t i = 0; i < outputCount_; ++i) {
    if (outputs_[i] == &output) {
      return;
    }
  }

  if (outputCount_ >= kMaxOutputs) {
    return;
  }

  outputs_[outputCount_++] = &output;
}

bool BacnetLogger::removeOutput(BacnetLogOutput& output) {
  for (size_t i = 0; i < outputCount_; ++i) {
    if (outputs_[i] == &output) {
      for (size_t j = i; j + 1 < outputCount_; ++j) {
        outputs_[j] = outputs_[j + 1];
      }
      outputs_[--outputCount_] = nullptr;
      return true;
    }
  }
  return false;
}

void BacnetLogger::clearOutputs() {
  for (size_t i = 0; i < outputCount_; ++i) {
    outputs_[i] = nullptr;
  }
  outputCount_ = 0;
}

size_t BacnetLogger::outputCount() const {
  return outputCount_;
}

void BacnetLogger::setTag(const char* tag) {
  copyText(baseTag_, sizeof(baseTag_), tag);
}

void BacnetLogger::clearTag() {
  baseTag_[0] = '\0';
}

void BacnetLogger::pushTag(const char* tag) {
  if (tag != nullptr && tag[0] != '\0' && tagDepth_ < kMaxTagDepth) {
    copyText(tagStack_[tagDepth_], sizeof(tagStack_[tagDepth_]), tag);
    ++tagDepth_;
  }
}

void BacnetLogger::popTag() {
  if (tagDepth_ > 0) {
    --tagDepth_;
    tagStack_[tagDepth_][0] = '\0';
  }
}

BacnetScopedLogTag BacnetLogger::scopedTag(const char* tag) {
  return BacnetScopedLogTag(this, tag);
}

void BacnetLogger::emit(const BacnetLogRecord& record) {
#if BACNET_ENABLE_LOGGING
  if (!shouldEmit(record.level)) {
    return;
  }

  for (size_t i = 0; i < outputCount_; ++i) {
    BacnetLogOutput* output = outputs_[i];
    if (output != nullptr && output->shouldLog(record)) {
      output->log(record);
    }
  }
#else
  (void)record;
#endif
}

void BacnetLogger::tick(uint32_t nowMs) {
#if BACNET_ENABLE_LOGGING
  for (size_t i = 0; i < outputCount_; ++i) {
    BacnetLogOutput* output = outputs_[i];
    if (output != nullptr) {
      output->tick(nowMs);
    }
  }
#else
  (void)nowMs;
#endif
}

void BacnetLogger::logTag(BacnetLogLevel level, const char* tag, const char* format, ...) {
  va_list args;
  va_start(args, format);
  logV(level, tag, format, args);
  va_end(args);
}

void BacnetLogger::log(BacnetLogLevel level, const char* format, ...) {
  va_list args;
  va_start(args, format);
  logV(level, nullptr, format, args);
  va_end(args);
}

void BacnetLogger::fatal(const char* tag, const char* format, ...) {
  va_list args;
  va_start(args, format);
  logV(BacnetLogLevel::Fatal, tag, format, args);
  va_end(args);
}

void BacnetLogger::error(const char* tag, const char* format, ...) {
  va_list args;
  va_start(args, format);
  logV(BacnetLogLevel::Error, tag, format, args);
  va_end(args);
}

void BacnetLogger::warn(const char* tag, const char* format, ...) {
  va_list args;
  va_start(args, format);
  logV(BacnetLogLevel::Warn, tag, format, args);
  va_end(args);
}

void BacnetLogger::info(const char* tag, const char* format, ...) {
  va_list args;
  va_start(args, format);
  logV(BacnetLogLevel::Info, tag, format, args);
  va_end(args);
}

void BacnetLogger::debug(const char* tag, const char* format, ...) {
  va_list args;
  va_start(args, format);
  logV(BacnetLogLevel::Debug, tag, format, args);
  va_end(args);
}

void BacnetLogger::trace(const char* tag, const char* format, ...) {
  va_list args;
  va_start(args, format);
  logV(BacnetLogLevel::Trace, tag, format, args);
  va_end(args);
}

const char* BacnetLogger::levelName(BacnetLogLevel level) {
  switch (level) {
    case BacnetLogLevel::Fatal:
      return "FATAL";
    case BacnetLogLevel::Error:
      return "ERROR";
    case BacnetLogLevel::Warn:
      return "WARN";
    case BacnetLogLevel::Info:
      return "INFO";
    case BacnetLogLevel::Debug:
      return "DEBUG";
    case BacnetLogLevel::Trace:
      return "TRACE";
    case BacnetLogLevel::Off:
    default:
      return "OFF";
  }
}

bool BacnetLogger::isEnabledFor(BacnetLogLevel level) {
#if !BACNET_ENABLE_LOGGING
  (void)level;
  return false;
#elif !BACNET_ENABLE_VERBOSE_LOGGING
  return !isVerboseLevel(level);
#else
  return true;
#endif
}

void BacnetLogger::logV(BacnetLogLevel level, const char* tag, const char* format, va_list args) {
#if BACNET_ENABLE_LOGGING
  if (format == nullptr || !shouldEmit(level)) {
    return;
  }

  char message[kMaxMessageLength] = {};
  std::vsnprintf(message, sizeof(message), format, args);

  char resolvedTag[kMaxTagLength] = {};
  buildTag(tag, resolvedTag, sizeof(resolvedTag));

  const BacnetLogRecord record{level, resolvedTag, message, millis(), "BACnet", nullptr};
  emit(record);
#else
  (void)level;
  (void)tag;
  (void)format;
  (void)args;
#endif
}

bool BacnetLogger::shouldEmit(BacnetLogLevel level) const {
  if (!isEnabledFor(level)) {
    return false;
  }
  return isLevelAllowed(level, globalLevel_);
}

void BacnetLogger::buildTag(const char* explicitTag, char* buffer, size_t bufferSize) const {
  if (buffer == nullptr || bufferSize == 0) {
    return;
  }

  buffer[0] = '\0';
  size_t used = 0;
  auto append = [&](const char* value) {
    if (value == nullptr || value[0] == '\0' || used >= bufferSize - 1) {
      return;
    }
    if (used != 0 && used < bufferSize - 1) {
      buffer[used++] = '/';
      buffer[used] = '\0';
    }
    const size_t remaining = bufferSize - used - 1;
    const size_t length = std::strlen(value);
    const size_t copyLength = length < remaining ? length : remaining;
    std::memcpy(&buffer[used], value, copyLength);
    used += copyLength;
    buffer[used] = '\0';
  };

  append(baseTag_);
  for (size_t i = 0; i < tagDepth_; ++i) {
    append(tagStack_[i]);
  }
  append(explicitTag);

  if (used == 0) {
    append("BACnet");
  }
}
