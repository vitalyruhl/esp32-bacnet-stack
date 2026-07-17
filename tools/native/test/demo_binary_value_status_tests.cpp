// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetDemoBinaryValueStatus.h"

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

BacnetValue binaryValue(uint32_t value) {
  BacnetValue result;
  result.type = BacnetValueType::Enumerated;
  result.unsignedValue = value;
  return result;
}

void testBinaryValueStateMapping() {
  BacnetDemoBinaryValueStatus status(320, 3000, 1000);

  status.applyReadResult(BacnetDeviceSessionReadStatus::Ack, binaryValue(0));
  expect(status.state() == BacnetDemoBinaryValueState::Inactive,
         "inactive BV Present_Value must switch the lamp off");
  expect(!status.isActive(), "inactive BV Present_Value must not be active");

  status.applyReadResult(BacnetDeviceSessionReadStatus::Ack, binaryValue(1));
  expect(status.state() == BacnetDemoBinaryValueState::Active,
         "active BV Present_Value must switch the lamp on");
  expect(status.isActive(), "active BV Present_Value must be active");

  status.applyReadResult(BacnetDeviceSessionReadStatus::Ack, binaryValue(2));
  expect(status.state() == BacnetDemoBinaryValueState::Unknown,
         "unsupported BV Present_Value must be unknown");
  expect(std::strcmp(status.detailText(), "unsupported value") == 0,
         "unsupported BV Present_Value must retain an explanatory detail");

  status.applyReadResult(BacnetDeviceSessionReadStatus::Timeout, BacnetValue{});
  expect(status.state() == BacnetDemoBinaryValueState::Unknown,
         "timed out BV reads must be unknown");
  expect(std::strcmp(status.detailText(), "timeout") == 0,
         "timed out BV reads must retain the read status");
}

void testWriteReadbackPolicyAndInstanceReset() {
  BacnetDemoBinaryValueStatus status(320, 3000, 1000);
  status.applyReadResult(BacnetDeviceSessionReadStatus::Ack, binaryValue(1));

  BacnetClient client;
  BacnetDeviceSession session(client, 1682101, BacnetIpEndpoint{});
  status.readBackAfterWrite(session, BacnetDeviceSessionWriteStatus::Timeout);
  expect(status.state() == BacnetDemoBinaryValueState::Active,
         "a failed write must not trigger a BV readback or replace the state");

  status.readBackAfterWrite(session, BacnetDeviceSessionWriteStatus::Ack);
  expect(status.state() == BacnetDemoBinaryValueState::Unknown,
         "an acknowledged Set 0 readback must use the actual read result");
  expect(status.lastReadStatus() == BacnetDeviceSessionReadStatus::SendFailed,
         "an acknowledged Set 0 must start a Present_Value readback");

  status.applyReadResult(BacnetDeviceSessionReadStatus::Ack, binaryValue(0));
  status.readBackAfterWrite(session, BacnetDeviceSessionWriteStatus::Ack);
  expect(status.lastReadStatus() == BacnetDeviceSessionReadStatus::SendFailed,
         "an acknowledged Set 1 must start a Present_Value readback");

  status.applyReadResult(BacnetDeviceSessionReadStatus::Ack, binaryValue(1));
  status.readBackAfterWrite(session, BacnetDeviceSessionWriteStatus::Ack);
  expect(status.lastReadStatus() == BacnetDeviceSessionReadStatus::SendFailed,
         "an acknowledged Relinquish must start a Present_Value readback");

  status.configure(321);
  expect(status.objectInstance() == 321,
         "changing the configured BV instance must select the new object");
  expect(status.state() == BacnetDemoBinaryValueState::Unknown,
         "changing the configured BV instance must clear the old state");

  status.applyReadResult(BacnetDeviceSessionReadStatus::Error, BacnetValue{});
  expect(status.state() == BacnetDemoBinaryValueState::Unknown,
         "a failed Set 0/Set 1/Relinquish readback must not assume a target value");
}

} // namespace

int main() {
  testBinaryValueStateMapping();
  testWriteReadbackPolicyAndInstanceReset();
  return failures == 0 ? 0 : 1;
}
