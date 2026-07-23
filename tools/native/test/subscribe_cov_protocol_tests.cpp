// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "portable/BacnetProtocol.h"

#include <cstdio>
#include <cstring>

namespace {

bool expectRequest(const uint8_t* expected,
                   size_t expectedSize,
                   uint32_t processId,
                   BacnetObjectId object,
                   uint32_t lifetimeSeconds,
                   bool issueConfirmedNotifications) {
  uint8_t actual[BacnetProtocol::kMaxSubscribeCovRequestSize] = {};
  const size_t actualSize = BacnetProtocol::buildSubscribeCovRequest(
    actual, sizeof(actual), processId, object, lifetimeSeconds,
    issueConfirmedNotifications);
  return actualSize == expectedSize &&
         std::memcmp(actual, expected, expectedSize) == 0;
}

bool expect(bool condition, const char* name) {
  if (condition) {
    return true;
  }
  std::fprintf(stderr, "[E] %s\n", name);
  return false;
}

}  // namespace

int main() {
  const BacnetObjectId analogValue200{
    static_cast<uint16_t>(BacnetObjectType::AnalogValue), 200};
  const uint8_t unconfirmed120[] = {
    0x81, 0x0A, 0x00, 0x15, 0x01, 0x04, 0x00, 0x05, 0x00, 0x05,
    0x09, 0x07, 0x1C, 0x00, 0x80, 0x00, 0xC8, 0x29, 0x00, 0x39,
    0x78};
  const uint8_t confirmed120[] = {
    0x81, 0x0A, 0x00, 0x15, 0x01, 0x04, 0x00, 0x05, 0x00, 0x05,
    0x09, 0x07, 0x1C, 0x00, 0x80, 0x00, 0xC8, 0x29, 0x01, 0x39,
    0x78};
  const uint8_t lifetimeZero[] = {
    0x81, 0x0A, 0x00, 0x15, 0x01, 0x04, 0x00, 0x05, 0x00, 0x05,
    0x09, 0x07, 0x1C, 0x00, 0x80, 0x00, 0xC8, 0x29, 0x00, 0x39,
    0x00};
  const uint8_t differentProcessAndObject[] = {
    0x81, 0x0A, 0x00, 0x16, 0x01, 0x04, 0x00, 0x05, 0x00, 0x05,
    0x0A, 0x12, 0x34, 0x1C, 0x01, 0x40, 0x00, 0x2A, 0x29, 0x01,
    0x39, 0x78};

  bool passed = true;
  passed &= expect(expectRequest(unconfirmed120, sizeof(unconfirmed120), 7,
                                  analogValue200, 120, false),
                   "unconfirmed SubscribeCOV request");
  passed &= expect(expectRequest(confirmed120, sizeof(confirmed120), 7,
                                  analogValue200, 120, true),
                   "confirmed SubscribeCOV request");
  passed &= expect(expectRequest(lifetimeZero, sizeof(lifetimeZero), 7,
                                  analogValue200, 0, false),
                   "zero-lifetime SubscribeCOV request");
  passed &= expect(expectRequest(
    differentProcessAndObject, sizeof(differentProcessAndObject), 0x1234,
    BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::BinaryValue), 42},
    120, true), "different process and object identifiers");

  uint8_t tooSmall[sizeof(unconfirmed120) - 1] = {};
  passed &= expect(BacnetProtocol::buildSubscribeCovRequest(
                     tooSmall, sizeof(tooSmall), 7, analogValue200, 120) == 0,
                   "insufficient SubscribeCOV request buffer");
  passed &= expect(BacnetProtocol::buildSubscribeCovRequest(
                     nullptr, 0, 7, analogValue200, 120) == 0,
                   "null SubscribeCOV request buffer");

  uint8_t propertyRequest[BacnetProtocol::kMaxSubscribeCovRequestSize] = {};
  const size_t propertyRequestSize = BacnetProtocol::buildSubscribeCovPropertyRequest(
    propertyRequest, sizeof(propertyRequest), 12, analogValue200,
    BacnetPropertyId::PresentValue, 60, false, kBacnetNoArrayIndex, true, 0.5F);
  BacnetSubscribeCovRequestHeader parsedPropertyRequest;
  passed &= expect(propertyRequestSize != 0 &&
                     BacnetProtocol::parseSubscribeCovRequest(
                       propertyRequest, propertyRequestSize, parsedPropertyRequest) ==
                       BacnetSubscribeCovRequestParseStatus::SubscribeCovProperty &&
                     parsedPropertyRequest.hasCovIncrement &&
                     parsedPropertyRequest.covIncrement == 0.5F,
                   "SubscribeCOVProperty COV_Increment round trip");

  const uint8_t reject[] = {
    0x81, 0x0A, 0x00, 0x09, 0x01, 0x00, 0x60, 0x07, 0x04};
  uint8_t rejectReason = 0xFF;
  passed &= expect(BacnetProtocol::classifySubscribeCovResponse(
                     reject, sizeof(reject), 7, &rejectReason) ==
                     BacnetSubscribeCovResponseKind::Reject &&
                     rejectReason == 4 &&
                     std::strcmp(BacnetProtocol::rejectReasonText(rejectReason),
                                 "invalid-tag") == 0,
                   "SubscribeCOV reject reason");

  const uint8_t covNotification[] = {
    0x81, 0x0A, 0x00, 0x28, 0x01, 0x00, 0x10, 0x02, 0x09, 0x07,
    0x1C, 0x02, 0x19, 0xAA, 0xB5, 0x2C, 0x00, 0x80, 0x00, 0xC8,
    0x39, 0x78, 0x4E, 0x09, 0x55, 0x2E, 0x44, 0x42, 0x2A, 0x00,
    0x00, 0x2F, 0x09, 0x6F, 0x2E, 0x82, 0x04, 0x00, 0x2F, 0x4F};
  BacnetCovNotification parsedNotification;
  const bool parsedStandardCov = BacnetProtocol::parseCovNotification(
    covNotification, sizeof(covNotification), parsedNotification);
  passed &= expect(parsedStandardCov, "standard unconfirmed COV notification parse");
  passed &= expect(parsedStandardCov && parsedNotification.processId == 7,
                   "standard unconfirmed COV process identifier");
  passed &= expect(parsedStandardCov &&
                     parsedNotification.object.type ==
                       static_cast<uint16_t>(BacnetObjectType::AnalogValue) &&
                     parsedNotification.object.instance == 200,
                   "standard unconfirmed COV monitored object");
  passed &= expect(parsedStandardCov &&
                     parsedNotification.property == BacnetPropertyId::PresentValue &&
                     parsedNotification.value.type == BacnetValueType::Real &&
                     parsedNotification.value.realValue == 42.5F,
                   "standard unconfirmed COV Present Value");

  const uint8_t covNotificationWithPriority[] = {
    0x81, 0x0A, 0x00, 0x2A, 0x01, 0x00, 0x10, 0x02, 0x09, 0x07,
    0x1C, 0x02, 0x19, 0xAA, 0xB5, 0x2C, 0x00, 0x80, 0x00, 0xC8,
    0x39, 0x78, 0x4E, 0x09, 0x55, 0x2E, 0x44, 0x42, 0x2A, 0x00,
    0x00, 0x2F, 0x39, 0x10, 0x09, 0x6F, 0x2E, 0x82, 0x04, 0x00,
    0x2F, 0x4F};
  passed &= expect(BacnetProtocol::parseCovNotification(
                     covNotificationWithPriority,
                     sizeof(covNotificationWithPriority), parsedNotification) &&
                     parsedNotification.property == BacnetPropertyId::PresentValue &&
                     parsedNotification.value.realValue == 42.5F,
                   "unconfirmed COV notification with priority");
  return passed ? 0 : 1;
}
