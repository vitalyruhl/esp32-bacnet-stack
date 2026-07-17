// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetDemoPropertyBrowser.h"

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

class FakeClock final : public BacnetMonotonicClock {
public:
  uint32_t nowMs() const override { return now; }

  uint32_t now = 0;
};

class FakeTransport final : public BacnetDatagramTransport {
public:
  bool begin(uint16_t) override { return true; }
  void end() override {}
  bool send(const BacnetIpEndpoint&, const uint8_t*, size_t) override {
    ++sendCount;
    return true;
  }
  size_t receive(uint8_t* data, size_t capacity, BacnetIpEndpoint&) override {
    if (responseSize == 0 || responseSize > capacity) {
      return 0;
    }
    std::memcpy(data, response, responseSize);
    const size_t size = responseSize;
    responseSize = 0;
    return size;
  }
  void idle() override {}

  unsigned sendCount = 0;
  uint8_t response[64] = {};
  size_t responseSize = 0;
};

size_t buildReadAck(uint8_t* buffer,
                    uint8_t invokeId,
                    BacnetObjectId object,
                    BacnetPropertyId property,
                    uint32_t arrayIndex,
                    const uint8_t* value,
                    size_t valueLength) {
  size_t offset = 0;
  buffer[offset++] = 0x81;
  buffer[offset++] = 0x0A;
  buffer[offset++] = 0;
  buffer[offset++] = 0;
  buffer[offset++] = 0x01;
  buffer[offset++] = 0x00;
  buffer[offset++] = 0x30;
  buffer[offset++] = invokeId;
  buffer[offset++] = 0x0C;
  buffer[offset++] = 0x0C;
  buffer[offset++] = static_cast<uint8_t>(object.type >> 8);
  buffer[offset++] = static_cast<uint8_t>(object.type << 6 | object.instance >> 16);
  buffer[offset++] = static_cast<uint8_t>(object.instance >> 8);
  buffer[offset++] = static_cast<uint8_t>(object.instance);
  const uint32_t propertyValue = static_cast<uint32_t>(property);
  if (propertyValue <= 0xFF) {
    buffer[offset++] = 0x19;
    buffer[offset++] = static_cast<uint8_t>(propertyValue);
  } else {
    buffer[offset++] = 0x1A;
    buffer[offset++] = static_cast<uint8_t>(propertyValue >> 8);
    buffer[offset++] = static_cast<uint8_t>(propertyValue);
  }
  if (arrayIndex != kBacnetNoArrayIndex) {
    buffer[offset++] = 0x29;
    buffer[offset++] = static_cast<uint8_t>(arrayIndex);
  }
  buffer[offset++] = 0x3E;
  std::memcpy(buffer + offset, value, valueLength);
  offset += valueLength;
  buffer[offset++] = 0x3F;
  buffer[2] = static_cast<uint8_t>(offset >> 8);
  buffer[3] = static_cast<uint8_t>(offset);
  return offset;
}

void testIncrementalLoadScheduling() {
  FakeTransport transport;
  FakeClock clock;
  BacnetClient client(transport, &clock);
  expect(client.begin(), "fake client must start");
  const BacnetObjectId object{
    static_cast<uint16_t>(BacnetObjectType::AnalogValue), 200};
  BacnetDeviceSession session(client, 1234, BacnetIpEndpoint(192, 0, 2, 1));
  BacnetDemoPropertyBrowser browser;

  expect(browser.queueLoad(object, 100), "browser load must queue");
  expect(browser.loadState() == BacnetDemoPropertyBrowser::LoadState::Queued,
         "browser must expose queued before its first transaction");
  browser.poll(session, clock.now);
  expect(transport.sendCount == 1,
         "first browser poll must start only the Property_List count request");
  browser.poll(session, clock.now);
  expect(transport.sendCount == 1,
         "an in-flight request must not start another browser transaction");

  const uint8_t countValue[] = {0x21, 0x01};
  transport.responseSize = buildReadAck(
    transport.response, 1, object, BacnetPropertyId::PropertyList, 0,
    countValue, sizeof(countValue));
  browser.poll(session, clock.now);
  expect(transport.sendCount == 1,
         "processing a completed request must not start another request in the same poll");
  browser.poll(session, clock.now);
  expect(transport.sendCount == 1,
         "state transition must remain request-free");
  browser.poll(session, clock.now);
  expect(transport.sendCount == 2,
         "the next Property_List entry starts on a later browser poll");
}

void testBusyAndCancelledLoad() {
  FakeTransport transport;
  FakeClock clock;
  BacnetClient client(transport, &clock);
  expect(client.begin(), "fake client must start");
  const BacnetObjectId object{
    static_cast<uint16_t>(BacnetObjectType::AnalogValue), 200};
  BacnetDeviceSession session(client, 1234, BacnetIpEndpoint(192, 0, 2, 1));
  BacnetPropertyReadJob blocker;
  expect(session.beginPropertyRead(
           blocker,
           BacnetPropertyRequest{object, BacnetPropertyId::ObjectName},
           100,
           clock.now),
         "test blocker must start");
  BacnetDemoPropertyBrowser browser;
  expect(browser.queueLoad(object, 100), "browser load must queue behind busy session");
  browser.poll(session, clock.now);
  expect(transport.sendCount == 1,
         "a busy session must not let the browser start a second transaction");
  browser.cancelLoad(session);
  expect(browser.loadState() == BacnetDemoPropertyBrowser::LoadState::Cancelled,
         "cancelling a queued browser job must leave no active browser job");
  session.cancelPropertyRead(blocker);
}

void testTimedOutPropertyListFailsWithoutRetryLoop() {
  FakeTransport transport;
  FakeClock clock;
  BacnetClient client(transport, &clock);
  expect(client.begin(), "fake client must start");
  BacnetDeviceSession session(client, 1234, BacnetIpEndpoint(192, 0, 2, 1));
  BacnetDemoPropertyBrowser browser;
  expect(browser.queueLoad(
           BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::AnalogValue), 200},
           10),
         "browser load must queue");
  browser.poll(session, 0);
  browser.poll(session, 11);
  browser.poll(session, 11);
  expect(browser.loadState() == BacnetDemoPropertyBrowser::LoadState::Failed,
         "a Property_List timeout must fail the browser job visibly");
  expect(transport.sendCount == 1,
         "a timed-out Property_List request must not enter a retry loop");
}

void testBoundedRowsAndSelection() {
  BacnetDemoPropertyBrowser browser;
  BacnetPropertyReadAllResult summary;
  summary.propertyListStatus = BacnetPropertyReadStatus::Ack;
  summary.advertised = 12;
  summary.collected = 8;
  summary.stored = 8;
  summary.truncated = true;

  BacnetPropertyReadResult rows[BacnetDemoPropertyBrowser::kMaxProperties] = {};
  rows[0].propertyId = BacnetPropertyId::PresentValue;
  rows[0].status = BacnetPropertyReadStatus::Ack;
  rows[0].value.type = BacnetValueType::Real;
  rows[0].value.realValue = 42.5F;
  rows[1].propertyId = BacnetPropertyId::MinPresentValue;
  rows[1].status = BacnetPropertyReadStatus::UnsupportedProperty;
  rows[2].propertyId = BacnetPropertyId::MaxPresentValue;
  rows[2].status = BacnetPropertyReadStatus::UnsupportedDatatype;
  rows[3].propertyId = BacnetPropertyId::Description;
  rows[3].status = BacnetPropertyReadStatus::EmptyValue;
  rows[4].propertyId = BacnetPropertyId::ObjectName;
  rows[4].status = BacnetPropertyReadStatus::DecodeError;
  rows[5].propertyId = BacnetPropertyId::StatusFlags;
  rows[5].status = BacnetPropertyReadStatus::Timeout;
  rows[6].propertyId = BacnetPropertyId::Reliability;
  rows[6].status = BacnetPropertyReadStatus::Reject;
  rows[7].propertyId = BacnetPropertyId::OutOfService;
  rows[7].status = BacnetPropertyReadStatus::ArrayIndexNotSupported;

  browser.applyReadAllResult(
    BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::AnalogValue), 200}, summary, rows,
    BacnetDemoPropertyBrowser::kMaxProperties);

  expect(browser.loaded(), "an acknowledged property list must load the browser");
  expect(browser.rowCount() == BacnetDemoPropertyBrowser::kMaxProperties,
         "the browser must retain only its bounded row capacity");
  expect(browser.result().truncated,
         "the browser must preserve the bounded-read indication");
  const BacnetPropertyReadResult* presentValue = browser.row(0);
  expect(presentValue != nullptr && presentValue->value.type == BacnetValueType::Real &&
           presentValue->value.realValue == 42.5F,
         "browser rows must retain the typed BACnet value without display substitution");
  expect(browser.select(0), "the first collected property must be selectable");
  expect(browser.selectedRow() == presentValue,
         "the selected property must resolve to the retained typed row");
  expect(!browser.select(BacnetDemoPropertyBrowser::kMaxProperties),
         "an out-of-range property selection must be rejected");
  expect(!browser.hasSelection(),
         "an invalid selection must not retain an active property selection");
  expect(std::strcmp(browser.subscriptionModeText(), "disabled") == 0,
         "a browser without an explicit subscription must report disabled");
}

void testPropertyFailureTextCoverage() {
  struct StatusExpectation {
    BacnetPropertyReadStatus status;
    const char* text;
  };
  const StatusExpectation expectations[] = {
    {BacnetPropertyReadStatus::Ack, "ok"},
    {BacnetPropertyReadStatus::UnsupportedProperty, "unsupported-property"},
    {BacnetPropertyReadStatus::UnsupportedDatatype, "unsupported-datatype"},
    {BacnetPropertyReadStatus::EmptyValue, "empty"},
    {BacnetPropertyReadStatus::DecodeError, "decode-error"},
    {BacnetPropertyReadStatus::Timeout, "timeout"},
    {BacnetPropertyReadStatus::Error, "error"},
    {BacnetPropertyReadStatus::Reject, "reject"},
    {BacnetPropertyReadStatus::Abort, "abort"},
    {BacnetPropertyReadStatus::ArrayIndexNotSupported, "array-index-not-supported"},
    {BacnetPropertyReadStatus::SendFailed, "send-failed"},
    {BacnetPropertyReadStatus::Busy, "busy"},
    {BacnetPropertyReadStatus::Skipped, "skipped"},
  };
  for (const StatusExpectation& expected : expectations) {
    expect(std::strcmp(bacnetPropertyReadStatusText(expected.status), expected.text) == 0,
           "property status must retain its distinct browser-facing text");
  }
}

void testRowsSurvivePropertyListFailure() {
  BacnetDemoPropertyBrowser browser;
  BacnetPropertyReadAllResult summary;
  summary.propertyListStatus = BacnetPropertyReadStatus::Error;
  summary.stored = 2;
  summary.failed = 1;

  BacnetPropertyReadResult rows[2] = {};
  rows[0].propertyId = BacnetPropertyId::PresentValue;
  rows[0].status = BacnetPropertyReadStatus::Ack;
  rows[0].value.type = BacnetValueType::Unsigned;
  rows[0].value.unsignedValue = 4;
  rows[1].propertyId = BacnetPropertyId::Description;
  rows[1].status = BacnetPropertyReadStatus::UnsupportedProperty;

  browser.applyReadAllResult(
    BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::MultiStateValue), 2020},
    summary, rows, 2);

  expect(browser.loaded(),
         "collected rows must remain displayable when a property-list request fails");
  expect(browser.rowCount() == 2,
         "a property-list failure must not discard independent property rows");
  expect(browser.row(0) != nullptr && browser.row(0)->value.type == BacnetValueType::Unsigned,
         "the browser must preserve the successful typed value beside a failed row");
  expect(browser.row(1) != nullptr &&
           browser.row(1)->status == BacnetPropertyReadStatus::UnsupportedProperty,
         "the browser must preserve the independent failed-property status");
}

}  // namespace

int main() {
  testBoundedRowsAndSelection();
  testPropertyFailureTextCoverage();
  testRowsSurvivePropertyListFailure();
  testIncrementalLoadScheduling();
  testBusyAndCancelledLoad();
  testTimedOutPropertyListFailsWithoutRetryLoop();
  return failures == 0 ? 0 : 1;
}
