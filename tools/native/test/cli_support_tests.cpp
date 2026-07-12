// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetCliSupport.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

int main() {
  uint32_t value = 0;
  BacnetIpEndpoint endpoint(0, 0, 0, 0, 47808);
  if (!bacnetCliParseUnsigned("47808", value) || value != 47808 ||
      bacnetCliParseUnsigned("", value) || bacnetCliParseUnsigned("-1", value) ||
      bacnetCliParseUnsigned("4294967296", value) ||
      !bacnetCliParseIpv4("192.168.2.101", endpoint) ||
      endpoint.address[0] != 192 || endpoint.address[1] != 168 ||
      endpoint.address[2] != 2 || endpoint.address[3] != 101 || endpoint.port != 47808 ||
      bacnetCliParseIpv4("192.168.2.256", endpoint) ||
      bacnetCliParseIpv4("192.168.2", endpoint) ||
      std::strcmp(bacnetCliExitCodeText(BacnetCliExitCode::InvalidArguments),
                  "invalid arguments") != 0 ||
      std::strcmp(bacnetCliExitCodeText(BacnetCliExitCode::Timeout), "timeout") != 0) {
    std::fputs("[E] CLI support test failed\n", stderr);
    return 1;
  }
  return 0;
}
