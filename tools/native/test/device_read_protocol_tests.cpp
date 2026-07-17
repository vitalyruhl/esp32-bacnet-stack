// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "portable/BacnetProtocol.h"

#include <cstdio>
#include <cstring>

namespace {

int failures = 0;

bool expect(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "[E] %s\n", message);
    ++failures;
  }
  return condition;
}

void testDevice1682101ReadRequests() {
  const BacnetObjectId device{
    static_cast<uint16_t>(BacnetObjectType::Device), 1682101};
  uint8_t request[BacnetProtocol::kMaxReadPropertyRequestSize] = {};
  const uint8_t objectNameExpected[] = {
    0x81, 0x0A, 0x00, 0x11, 0x01, 0x04, 0x00, 0x05, 0x5A,
    0x0C, 0x0C, 0x02, 0x19, 0xAA, 0xB5, 0x19, 0x4D,
  };
  const uint8_t objectListCountExpected[] = {
    0x81, 0x0A, 0x00, 0x13, 0x01, 0x04, 0x00, 0x05, 0x5B,
    0x0C, 0x0C, 0x02, 0x19, 0xAA, 0xB5, 0x19, 0x4C, 0x29,
    0x00,
  };

  const size_t objectNameLength = BacnetProtocol::buildReadPropertyRequest(
    request, sizeof(request), device, BacnetPropertyId::ObjectName, 0x5A);
  expect(objectNameLength == sizeof(objectNameExpected),
         "device object-name request length must match BVLC length");
  expect(std::memcmp(request, objectNameExpected, sizeof(objectNameExpected)) == 0,
         "device object-name request must encode type 8 and instance 1682101");

  const size_t objectListCountLength = BacnetProtocol::buildReadPropertyRequest(
    request,
    sizeof(request),
    device,
    BacnetPropertyId::ObjectList,
    0x5B,
    0);
  expect(objectListCountLength == sizeof(objectListCountExpected),
         "device object-list count request length must match BVLC length");
  expect(std::memcmp(
           request, objectListCountExpected, sizeof(objectListCountExpected)) == 0,
         "device object-list count request must include array index zero");
}

void testKnownErrorNames() {
  const uint8_t unknownObjectResponse[] = {
    0x81, 0x0A, 0x00, 0x0D, 0x01, 0x00, 0x50, 0x42, 0x0C,
    0x91, 0x01, 0x91, 0x1F,
  };
  BacnetValue value;
  uint32_t errorClass = 0;
  uint32_t errorCode = 0;

  expect(BacnetProtocol::parseReadPropertyError(
           unknownObjectResponse,
           sizeof(unknownObjectResponse),
           0x42,
           value,
           &errorClass,
           &errorCode),
         "read-property error must retain its invoke ID association");
  expect(errorClass == 1 && errorCode == 31,
         "unknown-object error class and code must be retained");
  expect(std::strcmp(value.displayText(), "unknown-object (object/31)") == 0,
         "known BACnet errors must use the canonical text");
}

}  // namespace

int main() {
  testDevice1682101ReadRequests();
  testKnownErrorNames();
  return failures == 0 ? 0 : 1;
}
