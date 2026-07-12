// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetCliSupport.h"

#include <cerrno>
#include <cstdlib>
#include <limits>

bool bacnetCliParseUnsigned(const char* text, uint32_t& value) {
  if (text == nullptr || text[0] == '\0' || text[0] == '-' || text[0] == '+') {
    return false;
  }

  char* end = nullptr;
  errno = 0;
  const unsigned long parsed = std::strtoul(text, &end, 10);
  if (errno != 0 || end == text || *end != '\0' ||
      parsed > std::numeric_limits<uint32_t>::max()) {
    return false;
  }

  value = static_cast<uint32_t>(parsed);
  return true;
}

bool bacnetCliParseIpv4(const char* text, BacnetIpEndpoint& endpoint) {
  if (text == nullptr || text[0] == '\0') {
    return false;
  }

  uint8_t bytes[4] = {};
  const char* current = text;
  for (size_t index = 0; index < 4; ++index) {
    if (*current < '0' || *current > '9') {
      return false;
    }
    uint32_t value = 0;
    do {
      value = value * 10U + static_cast<uint32_t>(*current - '0');
      if (value > 255U) {
        return false;
      }
      ++current;
    } while (*current >= '0' && *current <= '9');
    bytes[index] = static_cast<uint8_t>(value);
    if (index < 3) {
      if (*current != '.') {
        return false;
      }
      ++current;
    }
  }
  if (*current != '\0') {
    return false;
  }

  endpoint = BacnetIpEndpoint(bytes[0], bytes[1], bytes[2], bytes[3], endpoint.port);
  return true;
}

const char* bacnetCliExitCodeText(BacnetCliExitCode code) {
  switch (code) {
    case BacnetCliExitCode::Success:
      return "success";
    case BacnetCliExitCode::RuntimeFailure:
      return "runtime failure";
    case BacnetCliExitCode::Timeout:
      return "timeout";
    case BacnetCliExitCode::InvalidArguments:
      return "invalid arguments";
  }
  return "unknown";
}
