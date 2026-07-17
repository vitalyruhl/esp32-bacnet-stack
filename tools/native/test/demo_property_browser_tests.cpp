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
  return failures == 0 ? 0 : 1;
}
