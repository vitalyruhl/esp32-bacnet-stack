// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetCliSupport.h"
#include "BacnetNativeCli.h"
#include "portable/BacnetDisplayText.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

bool testValueDisplayText() {
  char text[BacnetValue::kMaxTextLength] = {};
  BacnetValue value;
  value.type = BacnetValueType::Null;
  if (std::strcmp(bacnetValueDisplayText(value, text, sizeof(text)), "null") != 0) return false;
  value.type = BacnetValueType::Boolean;
  value.booleanValue = true;
  if (std::strcmp(bacnetValueDisplayText(value, text, sizeof(text)), "true") != 0) return false;
  value.type = BacnetValueType::Unsigned;
  value.unsignedValue = 12;
  if (std::strcmp(bacnetValueDisplayText(value, text, sizeof(text)), "12") != 0) return false;
  value.type = BacnetValueType::Signed;
  value.signedValue = -3;
  if (std::strcmp(bacnetValueDisplayText(value, text, sizeof(text)), "-3") != 0) return false;
  value.type = BacnetValueType::Real;
  value.realValue = 12.5F;
  if (std::strcmp(bacnetValueDisplayText(value, text, sizeof(text)), "12.500") != 0) return false;
  value.type = BacnetValueType::Enumerated;
  value.unsignedValue = 7;
  if (std::strcmp(bacnetValueDisplayText(value, text, sizeof(text)), "7") != 0) return false;
  value.type = BacnetValueType::CharacterString;
  std::memcpy(value.text, "hello", 5);
  value.textLength = 5;
  if (std::strcmp(bacnetValueDisplayText(value, text, sizeof(text)), "hello") != 0) return false;
  value.type = BacnetValueType::ObjectIdentifier;
  value.objectValue = BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::AnalogValue), 42};
  return std::strcmp(bacnetValueDisplayText(value, text, sizeof(text)), "analog-value42") == 0;
}

}  // namespace

int main() {
  uint32_t value = 0;
  BacnetIpEndpoint endpoint(0, 0, 0, 0, 47808);
  BacnetObjectSelector selector;
  BacnetPropertyId property = BacnetPropertyId::ObjectName;
  uint32_t arrayIndex = kBacnetNoArrayIndex;
  if (!bacnetCliParseUnsigned("47808", value) || value != 47808 ||
      bacnetCliParseUnsigned("", value) || bacnetCliParseUnsigned("-1", value) ||
      bacnetCliParseUnsigned("4294967296", value) ||
      !bacnetCliParseIpv4("192.168.2.101", endpoint) ||
      endpoint.address[0] != 192 || endpoint.address[1] != 168 ||
      endpoint.address[2] != 2 || endpoint.address[3] != 101 || endpoint.port != 47808 ||
      bacnetCliParseIpv4("192.168.2.256", endpoint) ||
      bacnetCliParseIpv4("192.168.2", endpoint) ||
      !bacnetNativeParseObjectSelector("AV0", selector) ||
      selector.object.type != static_cast<uint16_t>(BacnetObjectType::AnalogValue) ||
      selector.object.instance != 0 ||
      !bacnetNativeParseObjectSelector("msv2000", selector) ||
      selector.object.type != static_cast<uint16_t>(BacnetObjectType::MultiStateValue) ||
      selector.object.instance != 2000 ||
      !bacnetNativeParseObjectSelector("device,1682101", selector) ||
      selector.object.type != static_cast<uint16_t>(BacnetObjectType::Device) ||
      selector.object.instance != 1682101 ||
      bacnetNativeParseObjectSelector("AV", selector) ||
      bacnetNativeParseObjectSelector("AV4294967296", selector) ||
      !bacnetNativeParseObjectPropertySelector("AV200.presentValue", selector, property) ||
      property != BacnetPropertyId::PresentValue ||
      !bacnetNativeParseObjectPropertySelector(
        "device,1682101.objectList[0]", selector, property, arrayIndex) ||
      property != BacnetPropertyId::ObjectList || arrayIndex != 0 ||
      selector.object.type != static_cast<uint16_t>(BacnetObjectType::Device) ||
      selector.object.instance != 1682101 ||
      !bacnetNativeParseObjectPropertySelector(
        "AV200.priorityArray[8]", selector, property, arrayIndex) ||
      property != BacnetPropertyId::PriorityArray || arrayIndex != 8 ||
      selector.object.type != static_cast<uint16_t>(BacnetObjectType::AnalogValue) ||
      selector.object.instance != 200 ||
      !bacnetNativeParseProperty("objectIdentifier", property) ||
      property != BacnetPropertyId::ObjectIdentifier ||
      !bacnetNativeParseProperty("protocolRevision", property) ||
      property != BacnetPropertyId::ProtocolRevision ||
      !bacnetNativeParseProperty("property-list", property) ||
      property != BacnetPropertyId::PropertyList ||
      !bacnetNativeParseProperty("statusFlags", property) ||
      property != BacnetPropertyId::StatusFlags ||
      !bacnetNativeParseProperty("units", property) ||
      property != BacnetPropertyId::Units ||
      bacnetNativeParseProperty("unknown-property", property) ||
      std::strcmp(bacnetCliExitCodeText(BacnetCliExitCode::InvalidArguments),
                  "invalid arguments") != 0 ||
      std::strcmp(bacnetCliExitCodeText(BacnetCliExitCode::Timeout), "timeout") != 0 ||
      !testValueDisplayText()) {
    std::fputs("[E] CLI support test failed\n", stderr);
    return 1;
  }
  return 0;
}
