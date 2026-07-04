// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstring>

struct FixedTextBuffer {
  char* data = nullptr;
  size_t capacity = 0;
  size_t length = 0;
  bool overflow = false;

  FixedTextBuffer(char* output, size_t outputCapacity)
      : data(output), capacity(outputCapacity) {
    if (capacity > 0) {
      data[0] = '\0';
    }
  }

  const char* c_str() const {
    return data != nullptr ? data : "";
  }

  bool empty() const {
    return length == 0;
  }

  void append(const char* text) {
    if (text == nullptr) {
      return;
    }
    const size_t available = capacity > length ? capacity - length : 0;
    if (available == 0) {
      overflow = true;
      return;
    }
    const int written = std::snprintf(data + length, available, "%s", text);
    updateLength(written, available);
  }

  void append(char value) {
    char text[2] = {value, '\0'};
    append(text);
  }

  void appendFormat(const char* format, ...) {
    const size_t available = capacity > length ? capacity - length : 0;
    if (available == 0) {
      overflow = true;
      return;
    }
    va_list args;
    va_start(args, format);
    const int written = std::vsnprintf(data + length, available, format, args);
    va_end(args);
    updateLength(written, available);
  }

  void appendShortened(const char* text, size_t maxLength = 36) {
    if (text == nullptr) {
      return;
    }
    const size_t sourceLength = std::strlen(text);
    if (sourceLength <= maxLength) {
      append(text);
      return;
    }
    if (maxLength <= 3) {
      append("...");
      return;
    }
    const size_t prefixLength = maxLength - 3;
    const size_t available = capacity > length ? capacity - length : 0;
    if (available == 0) {
      overflow = true;
      return;
    }
    const int written =
      std::snprintf(data + length,
                    available,
                    "%.*s...",
                    static_cast<int>(prefixLength),
                    text);
    updateLength(written, available);
  }

private:
  void updateLength(int written, size_t available) {
    if (written < 0) {
      overflow = true;
      if (capacity > 0) {
        data[capacity - 1] = '\0';
        length = std::strlen(data);
      }
      return;
    }
    if (static_cast<size_t>(written) >= available) {
      overflow = true;
      length = capacity > 0 ? capacity - 1 : 0;
      if (capacity > 0) {
        data[length] = '\0';
      }
      return;
    }
    length += static_cast<size_t>(written);
  }
};
