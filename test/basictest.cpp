// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include <BacnetClient.h>
#include <BacnetDeviceSession.h>
#include <BacnetRemoteObject.h>
#include <portable/BacnetProtocol.h>
#include <BacnetServer.h>
#include <EspBacnet.h>
#include <unity.h>

#include <cstdio>
#include <cstring>
#include <type_traits>
#include <utility>

struct SubscriptionCallbackCapture {
  size_t calls = 0;
  BacnetObjectId objectId;
  BacnetPropertyId propertyId = BacnetPropertyId::ObjectName;
  uint32_t arrayIndex = kBacnetNoArrayIndex;
  BacnetDeviceSessionReadStatus status = BacnetDeviceSessionReadStatus::Skipped;
  bool hasValue = false;
  bool firstValue = false;
  bool valueChanged = false;
  bool statusChanged = false;
  BacnetSubscriptionNotificationReason reason =
    BacnetSubscriptionNotificationReason::None;
};

static void captureSubscriptionNotification(
  const BacnetSubscriptionNotification& notification) {
  auto* capture =
    static_cast<SubscriptionCallbackCapture*>(notification.userData);
  if (capture == nullptr) {
    return;
  }

  ++capture->calls;
  capture->objectId = notification.objectId;
  capture->propertyId = notification.propertyId;
  capture->arrayIndex = notification.arrayIndex;
  capture->status = notification.status;
  capture->hasValue = notification.hasValue;
  capture->firstValue = notification.firstValue;
  capture->valueChanged = notification.valueChanged;
  capture->statusChanged = notification.statusChanged;
  capture->reason = notification.reason;
}

class CapturingBacnetLogOutput : public BacnetLogOutput {
public:
  void log(const BacnetLogRecord& record) override {
    ++count;
    lastLevel = record.level;
    lastTimestampMs = record.timestampMs;
    snprintf(lastTag, sizeof(lastTag), "%s", record.tag != nullptr ? record.tag : "");
    snprintf(lastMessage, sizeof(lastMessage), "%s", record.message != nullptr ? record.message : "");
  }

  size_t count = 0;
  BacnetLogLevel lastLevel = BacnetLogLevel::Off;
  uint32_t lastTimestampMs = 0;
  char lastTag[96] = {};
  char lastMessage[192] = {};
};

static bool acceptMatchingTag(const BacnetLogRecord& record,
                              const void* userData) {
  const char* expected = static_cast<const char*>(userData);
  return expected != nullptr && record.tag != nullptr &&
         strcmp(record.tag, expected) == 0;
}

void test_bacnet_client_lifecycle() {
  BacnetClient client;

  TEST_ASSERT_FALSE(client.isRunning());
  TEST_ASSERT_TRUE(client.begin(47809));
  TEST_ASSERT_TRUE(client.isRunning());
  TEST_ASSERT_EQUAL_UINT16(47809, client.localPort());

  client.end();
  TEST_ASSERT_FALSE(client.isRunning());
}

void test_bacnet_logger_filters_global_and_output_levels() {
  BacnetLogger logger;
  CapturingBacnetLogOutput infoOutput;
  CapturingBacnetLogOutput warnOutput;

  infoOutput.setLevel(BacnetLogLevel::Info);
  warnOutput.setLevel(BacnetLogLevel::Warn);
  logger.setGlobalLevel(BacnetLogLevel::Info);
  logger.addOutput(infoOutput);
  logger.addOutput(warnOutput);

  logger.info("BACnet/Test", "visible info");
  TEST_ASSERT_EQUAL_UINT32(1, infoOutput.count);
  TEST_ASSERT_EQUAL_UINT32(0, warnOutput.count);
  TEST_ASSERT_EQUAL_STRING("BACnet/Test", infoOutput.lastTag);
  TEST_ASSERT_EQUAL_STRING("visible info", infoOutput.lastMessage);

  logger.error("BACnet/Test", "visible error");
  TEST_ASSERT_EQUAL_UINT32(2, infoOutput.count);
  TEST_ASSERT_EQUAL_UINT32(1, warnOutput.count);

  logger.setGlobalLevel(BacnetLogLevel::Error);
  logger.info("BACnet/Test", "hidden info");
  TEST_ASSERT_EQUAL_UINT32(2, infoOutput.count);
  TEST_ASSERT_EQUAL_UINT32(1, warnOutput.count);
}

void test_bacnet_logger_filters_tags_and_rate_limits() {
  BacnetLogger logger;
  CapturingBacnetLogOutput output;
  const char expectedTag[] = "BACnet/Discovery";

  output.setLevel(BacnetLogLevel::Info);
  output.setFilter(acceptMatchingTag, expectedTag);
  output.setMinIntervalMs(100);
  logger.addOutput(output);

  logger.emit(BacnetLogRecord{BacnetLogLevel::Info, "BACnet/ReadProperty", "filtered", 100, "BACnet", nullptr});
  TEST_ASSERT_EQUAL_UINT32(0, output.count);

  logger.emit(BacnetLogRecord{BacnetLogLevel::Info, "BACnet/Discovery", "first", 100, "BACnet", nullptr});
  logger.emit(BacnetLogRecord{BacnetLogLevel::Info, "BACnet/Discovery", "rate limited", 150, "BACnet", nullptr});
  logger.emit(BacnetLogRecord{BacnetLogLevel::Info, "BACnet/Discovery", "second", 220, "BACnet", nullptr});

  TEST_ASSERT_EQUAL_UINT32(2, output.count);
  TEST_ASSERT_EQUAL_STRING("second", output.lastMessage);
}

void test_bacnet_logger_scoped_tags_and_no_output_mode() {
  BacnetLogger logger;
  CapturingBacnetLogOutput output;
  output.setLevel(BacnetLogLevel::Info);

  logger.info("BACnet/Test", "no output attached");

  logger.addOutput(output);
  logger.setTag("BACnet");
  {
    auto scoped = logger.scopedTag("ReadProperty");
    logger.info("ACK", "value %d", 7);
  }

  TEST_ASSERT_EQUAL_UINT32(1, output.count);
  TEST_ASSERT_EQUAL_STRING("BACnet/ReadProperty/ACK", output.lastTag);
  TEST_ASSERT_EQUAL_STRING("value 7", output.lastMessage);
}

void test_bacnet_logger_uses_bounded_output_storage() {
  BacnetLogger logger;
  CapturingBacnetLogOutput outputs[BacnetLogger::kMaxOutputs + 1];

  for (size_t i = 0; i < BacnetLogger::kMaxOutputs + 1; ++i) {
    logger.addOutput(outputs[i]);
  }

  TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(BacnetLogger::kMaxOutputs),
                           static_cast<uint32_t>(logger.outputCount()));

  if (BacnetLogger::kMaxOutputs > 0) {
    TEST_ASSERT_TRUE(logger.removeOutput(outputs[0]));
    TEST_ASSERT_EQUAL_UINT32(
      static_cast<uint32_t>(BacnetLogger::kMaxOutputs - 1),
      static_cast<uint32_t>(logger.outputCount()));

    logger.addOutput(outputs[BacnetLogger::kMaxOutputs]);
    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(BacnetLogger::kMaxOutputs),
                             static_cast<uint32_t>(logger.outputCount()));
  }
}

void test_bacnet_logger_ignores_extra_scoped_tags() {
  BacnetLogger logger;
  CapturingBacnetLogOutput output;
  output.setLevel(BacnetLogLevel::Info);
  logger.addOutput(output);
  logger.setTag("BACnet");

  for (size_t i = 0; i < BacnetLogger::kMaxTagDepth + 2; ++i) {
    logger.pushTag("Scope");
  }

  logger.info("ACK", "done");

  char expected[BacnetLogger::kMaxTagLength] = {};
  snprintf(expected, sizeof(expected), "BACnet");
  for (size_t i = 0; i < BacnetLogger::kMaxTagDepth; ++i) {
    strncat(expected, "/Scope", sizeof(expected) - strlen(expected) - 1);
  }
  strncat(expected, "/ACK", sizeof(expected) - strlen(expected) - 1);

  TEST_ASSERT_EQUAL_STRING(expected, output.lastTag);
}

void test_bacnet_logger_verbose_compile_gate() {
  BacnetLogger logger;
  CapturingBacnetLogOutput output;
  output.setLevel(BacnetLogLevel::Trace);
  logger.setGlobalLevel(BacnetLogLevel::Trace);
  logger.addOutput(output);

  logger.debug("BACnet/Test", "debug message");
  if (BacnetLogger::isEnabledFor(BacnetLogLevel::Debug)) {
    TEST_ASSERT_EQUAL_UINT32(1, output.count);
  } else {
    TEST_ASSERT_EQUAL_UINT32(0, output.count);
  }
}

void test_portable_protocol_builds_who_is_request() {
  uint8_t request[BacnetProtocol::kWhoIsRequestSize] = {};
  const uint8_t expected[] = {0x81, 0x0B, 0x00, 0x08, 0x01, 0x00, 0x10, 0x08};

  TEST_ASSERT_EQUAL_UINT32(sizeof(expected),
                           BacnetProtocol::buildWhoIsRequest(request,
                                                             sizeof(request)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, request, sizeof(expected));
  TEST_ASSERT_EQUAL_UINT32(0, BacnetProtocol::buildWhoIsRequest(nullptr, 0));
}

void test_portable_protocol_parses_i_am_response() {
  const uint8_t response[] = {
    0x81,
    0x0A,
    0x00,
    0x14,
    0x01,
    0x00,
    0x10,
    0x00,
    0xC4,
    0x02,
    0x00,
    0x04,
    0xD2,
    0x22,
    0x05,
    0xC4,
    0x91,
    0x00,
    0x21,
    0xDE,
  };
  BacnetIAmDeviceInfo device;

  TEST_ASSERT_TRUE(BacnetProtocol::parseIAmResponse(response, sizeof(response), device));
  TEST_ASSERT_EQUAL_UINT32(1234, device.deviceInstance);
  TEST_ASSERT_EQUAL_UINT32(1476, device.maxApduLengthAccepted);
  TEST_ASSERT_EQUAL_UINT8(0, device.segmentationSupported);
  TEST_ASSERT_EQUAL_UINT16(222, device.vendorId);
}

void test_bacnet_client_rejects_non_i_am_response() {
  const uint8_t response[] = {0x81, 0x0B, 0x00, 0x08, 0x01, 0x00, 0x10, 0x08};
  BacnetIAmDevice device;

  TEST_ASSERT_FALSE(BacnetClient::parseIAmResponse(response, sizeof(response), device));
}

void test_portable_protocol_classifies_reject_and_abort() {
  const uint8_t reject[] = {0x81, 0x0A, 0x00, 0x08, 0x01, 0x00, 0x60, 0x04};
  const uint8_t abort[] = {0x81, 0x0A, 0x00, 0x08, 0x01, 0x00, 0x70, 0x04};

  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetReadPropertyResponseKind::Reject),
    static_cast<uint8_t>(BacnetProtocol::classifyReadPropertyResponse(
      reject, sizeof(reject), 4)));
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetReadPropertyResponseKind::Abort),
    static_cast<uint8_t>(BacnetProtocol::classifyReadPropertyResponse(
      abort, sizeof(abort), 4)));
}

void test_bacnet_client_builds_read_property_request() {
  uint8_t request[BacnetClient::kMaxReadPropertyRequestSize] = {};
  const uint8_t expected[] = {
    0x81,
    0x0A,
    0x00,
    0x11,
    0x01,
    0x04,
    0x00,
    0x05,
    0x01,
    0x0C,
    0x0C,
    0x02,
    0x00,
    0x04,
    0xD2,
    0x19,
    0x4D,
  };

  TEST_ASSERT_EQUAL_UINT32(
    sizeof(expected),
    BacnetProtocol::buildReadPropertyRequest(
      request, sizeof(request), BacnetObjectId{8, 1234}, BacnetPropertyId::ObjectName, 1));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, request, sizeof(expected));
}

void test_bacnet_client_builds_ai_present_value_request_with_type_zero() {
  uint8_t request[BacnetClient::kMaxReadPropertyRequestSize] = {};
  const uint8_t expected[] = {
    0x81,
    0x0A,
    0x00,
    0x11,
    0x01,
    0x04,
    0x00,
    0x05,
    0x09,
    0x0C,
    0x0C,
    0x00,
    0x00,
    0x00,
    0x64,
    0x19,
    0x55,
  };

  TEST_ASSERT_EQUAL_UINT32(
    sizeof(expected),
    BacnetClient::buildReadPropertyRequest(
      request, sizeof(request), BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::AnalogInput), 100}, BacnetPropertyId::PresentValue, 9));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, request, sizeof(expected));
}

void test_bacnet_property_ids_cover_generic_access_slice() {
  TEST_ASSERT_EQUAL_UINT32(22,
                           static_cast<uint32_t>(BacnetPropertyId::CovIncrement));
  TEST_ASSERT_EQUAL_UINT32(28,
                           static_cast<uint32_t>(BacnetPropertyId::Description));
  TEST_ASSERT_EQUAL_UINT32(36,
                           static_cast<uint32_t>(BacnetPropertyId::EventState));
  TEST_ASSERT_EQUAL_UINT32(
    74, static_cast<uint32_t>(BacnetPropertyId::NumberOfStates));
  TEST_ASSERT_EQUAL_UINT32(65,
                           static_cast<uint32_t>(BacnetPropertyId::MaxPresentValue));
  TEST_ASSERT_EQUAL_UINT32(69,
                           static_cast<uint32_t>(BacnetPropertyId::MinPresentValue));
  TEST_ASSERT_EQUAL_UINT32(76,
                           static_cast<uint32_t>(BacnetPropertyId::ObjectList));
  TEST_ASSERT_EQUAL_UINT32(77,
                           static_cast<uint32_t>(BacnetPropertyId::ObjectName));
  TEST_ASSERT_EQUAL_UINT32(81,
                           static_cast<uint32_t>(BacnetPropertyId::OutOfService));
  TEST_ASSERT_EQUAL_UINT32(85,
                           static_cast<uint32_t>(BacnetPropertyId::PresentValue));
  TEST_ASSERT_EQUAL_UINT32(87,
                           static_cast<uint32_t>(BacnetPropertyId::PriorityArray));
  TEST_ASSERT_EQUAL_UINT32(103,
                           static_cast<uint32_t>(BacnetPropertyId::Reliability));
  TEST_ASSERT_EQUAL_UINT32(
    104, static_cast<uint32_t>(BacnetPropertyId::RelinquishDefault));
  TEST_ASSERT_EQUAL_UINT32(106,
                           static_cast<uint32_t>(BacnetPropertyId::Resolution));
  TEST_ASSERT_EQUAL_UINT32(110,
                           static_cast<uint32_t>(BacnetPropertyId::StateText));
  TEST_ASSERT_EQUAL_UINT32(111,
                           static_cast<uint32_t>(BacnetPropertyId::StatusFlags));
  TEST_ASSERT_EQUAL_UINT32(117,
                           static_cast<uint32_t>(BacnetPropertyId::Units));
  TEST_ASSERT_EQUAL_UINT32(371,
                           static_cast<uint32_t>(BacnetPropertyId::PropertyList));
}

void test_bacnet_engineering_unit_symbol_mapping_is_small_and_safe() {
  BacnetValue value;
  value.type = BacnetValueType::Enumerated;
  value.unsignedValue = 62;

  uint32_t unitId = 0;
  TEST_ASSERT_TRUE(bacnetEngineeringUnitId(value, unitId));
  TEST_ASSERT_EQUAL_UINT32(62, unitId);
  TEST_ASSERT_EQUAL_STRING("degC", bacnetEngineeringUnitSymbol(unitId));
  TEST_ASSERT_EQUAL_STRING("degC", bacnetCommonEngineeringUnitSymbol(unitId));
  TEST_ASSERT_EQUAL_STRING("\xC2\xB0", bacnetEngineeringUnitSymbol(90));
  TEST_ASSERT_EQUAL_STRING("%", bacnetEngineeringUnitSymbol(98));
  TEST_ASSERT_EQUAL_STRING("", bacnetEngineeringUnitSymbol(95));
  TEST_ASSERT_NULL(bacnetEngineeringUnitSymbol(9999));

  value.type = BacnetValueType::Real;
  TEST_ASSERT_FALSE(bacnetEngineeringUnitId(value, unitId));

  TEST_ASSERT_EQUAL_STRING("AV", bacnetObjectTypePrefix(BacnetObjectType::AnalogValue));
  TEST_ASSERT_EQUAL_STRING("DEV", bacnetObjectTypePrefix(BacnetObjectType::Device));
  TEST_ASSERT_EQUAL_STRING("OBJ", bacnetObjectTypePrefix(9999));
  TEST_ASSERT_TRUE(bacnetIsAnalogProcessObject(
    static_cast<uint16_t>(BacnetObjectType::AnalogInput)));
  TEST_ASSERT_TRUE(bacnetIsBinaryProcessObject(
    static_cast<uint16_t>(BacnetObjectType::BinaryValue)));
  TEST_ASSERT_TRUE(bacnetIsMultiStateProcessObject(
    static_cast<uint16_t>(BacnetObjectType::MultiStateValue)));
  TEST_ASSERT_TRUE(bacnetIsProcessObject(
    static_cast<uint16_t>(BacnetObjectType::AnalogValue)));
  TEST_ASSERT_FALSE(bacnetIsProcessObject(
    static_cast<uint16_t>(BacnetObjectType::Device)));
  TEST_ASSERT_EQUAL_STRING(
    "present-value",
    bacnetPropertyDisplayName(BacnetPropertyId::PresentValue));
  TEST_ASSERT_EQUAL_STRING(
    "cov-increment",
    bacnetPropertyDisplayName(BacnetPropertyId::CovIncrement));
  TEST_ASSERT_EQUAL_STRING(
    "property",
    bacnetPropertyDisplayName(static_cast<BacnetPropertyId>(9999)));

  BacnetSubscriptionNotification notification;
  TEST_ASSERT_EQUAL_STRING("update", bacnetSubscriptionReasonText(notification));
  notification.statusChanged = true;
  TEST_ASSERT_EQUAL_STRING("status", bacnetSubscriptionReasonText(notification));
  notification.valueChanged = true;
  TEST_ASSERT_EQUAL_STRING("changed", bacnetSubscriptionReasonText(notification));
  notification.firstValue = true;
  TEST_ASSERT_EQUAL_STRING("first", bacnetSubscriptionReasonText(notification));
}

void test_bacnet_display_value_and_status_helpers() {
  char objectText[16] = {};
  FixedTextBuffer objectOut(objectText, sizeof(objectText));
  bacnetAppendObjectDisplayName(
    objectOut,
    BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::AnalogValue), 200});
  TEST_ASSERT_EQUAL_STRING("AV200", objectOut.c_str());

  BacnetValue objectName;
  objectName.type = BacnetValueType::CharacterString;
  snprintf(objectName.text, sizeof(objectName.text), "Room Temperature");
  objectName.textLength = strlen(objectName.text);

  BacnetValue description;
  description.type = BacnetValueType::CharacterString;
  snprintf(description.text, sizeof(description.text), "Fallback Label");
  description.textLength = strlen(description.text);

  BacnetScannedObject scanned;
  scanned.objectNameStatus = BacnetDeviceSessionReadStatus::Ack;
  scanned.objectName = objectName;
  scanned.descriptionStatus = BacnetDeviceSessionReadStatus::Ack;
  scanned.description = description;
  TEST_ASSERT_EQUAL_STRING("Room Temperature",
                           bacnetScannedLabelOrNull(scanned));

  scanned.objectName.text[0] = '\0';
  scanned.objectName.textLength = 0;
  TEST_ASSERT_EQUAL_STRING("Fallback Label", bacnetScannedLabelOrNull(scanned));

  BacnetValue presentValue;
  presentValue.type = BacnetValueType::Real;
  presentValue.realValue = 12.5f;
  float numericValue = 0.0f;
  TEST_ASSERT_TRUE(bacnetValueAsFloat(presentValue, numericValue));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.5f, numericValue);
  TEST_ASSERT_TRUE(bacnetValueInRange(presentValue, 10.0f, 15.0f));
  TEST_ASSERT_FALSE(bacnetValueInRange(presentValue, 13.0f, 15.0f));

  BacnetObjectStatus status;
  status.state = BacnetObjectHealthState::Normal;
  status.statusFlagsStatus = BacnetPropertyReadStatus::Ack;
  status.outOfServiceStatus = BacnetPropertyReadStatus::Ack;
  TEST_ASSERT_TRUE(bacnetStatusIsNormal(status));
  TEST_ASSERT_FALSE(bacnetStatusHasAlarm(status));
  TEST_ASSERT_FALSE(bacnetStatusIsOutOfService(status));

  status.statusFlags.inAlarm = true;
  TEST_ASSERT_TRUE(bacnetStatusHasAlarm(status));
  TEST_ASSERT_FALSE(bacnetStatusIsNormal(status));

  status.statusFlags.inAlarm = false;
  status.outOfService = true;
  TEST_ASSERT_TRUE(bacnetStatusIsOutOfService(status));
}

void test_bacnet_client_builds_mv_present_value_request() {
  uint8_t request[BacnetClient::kMaxReadPropertyRequestSize] = {};
  const uint8_t expected[] = {
    0x81,
    0x0A,
    0x00,
    0x11,
    0x01,
    0x04,
    0x00,
    0x05,
    0x02,
    0x0C,
    0x0C,
    0x04,
    0xC0,
    0x00,
    0x03,
    0x19,
    0x55,
  };

  TEST_ASSERT_EQUAL_UINT32(
    sizeof(expected),
    BacnetClient::buildReadPropertyRequest(
      request, sizeof(request), BacnetObjectId{19, 3}, BacnetPropertyId::PresentValue, 2));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, request, sizeof(expected));
}

void test_bacnet_client_builds_generic_property_request() {
  uint8_t request[BacnetClient::kMaxReadPropertyRequestSize] = {};
  const uint8_t expected[] = {
    0x81,
    0x0A,
    0x00,
    0x13,
    0x01,
    0x04,
    0x00,
    0x05,
    0x06,
    0x0C,
    0x0C,
    0x04,
    0xC0,
    0x07,
    0xD0,
    0x19,
    0x6E,
    0x29,
    0x01,
  };
  const BacnetPropertyRequest propertyRequest{
    BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::MultiStateValue),
                   2000},
    BacnetPropertyId::StateText,
    1};

  TEST_ASSERT_EQUAL_UINT32(
    sizeof(expected),
    BacnetClient::buildReadPropertyRequest(
      request, sizeof(request), propertyRequest, 6));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, request, sizeof(expected));
}

void test_bacnet_client_builds_object_list_array_request() {
  uint8_t request[BacnetClient::kMaxReadPropertyRequestSize] = {};
  const uint8_t expected[] = {
    0x81,
    0x0A,
    0x00,
    0x13,
    0x01,
    0x04,
    0x00,
    0x05,
    0x03,
    0x0C,
    0x0C,
    0x02,
    0x00,
    0x04,
    0xD2,
    0x19,
    0x4C,
    0x29,
    0x02,
  };

  TEST_ASSERT_EQUAL_UINT32(
    sizeof(expected),
    BacnetClient::buildReadPropertyRequest(
      request, sizeof(request), BacnetObjectId{8, 1234}, BacnetPropertyId::ObjectList, 3, 2));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, request, sizeof(expected));
}

void test_bacnet_client_parses_read_property_ack() {
  const uint8_t response[] = {
    0x81,
    0x0A,
    0x00,
    0x19,
    0x01,
    0x00,
    0x30,
    0x01,
    0x0C,
    0x0C,
    0x02,
    0x00,
    0x04,
    0xD2,
    0x19,
    0x4D,
    0x3E,
    0x75,
    0x05,
    0x00,
    0x57,
    0x41,
    0x47,
    0x4F,
    0x3F,
  };
  BacnetValue value;

  TEST_ASSERT_TRUE(BacnetProtocol::parseReadPropertyAck(
    response, sizeof(response), 1, BacnetPropertyId::ObjectName, value));
  TEST_ASSERT_EQUAL_STRING("WAGO", value.text);
  TEST_ASSERT_EQUAL_UINT32(4, value.textLength);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetValueType::CharacterString),
                          static_cast<uint8_t>(value.type));
}

void test_bacnet_client_parses_mv_present_value_ack() {
  const uint8_t response[] = {
    0x81,
    0x0A,
    0x00,
    0x14,
    0x01,
    0x00,
    0x30,
    0x02,
    0x0C,
    0x0C,
    0x04,
    0xC0,
    0x00,
    0x03,
    0x19,
    0x55,
    0x3E,
    0x21,
    0x03,
    0x3F,
  };
  BacnetValue value;

  TEST_ASSERT_TRUE(BacnetClient::parseReadPropertyAck(
    response, sizeof(response), 2, BacnetPropertyId::PresentValue, value));
  TEST_ASSERT_EQUAL_STRING("3", value.text);
  TEST_ASSERT_EQUAL_UINT32(1, value.textLength);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetValueType::Enumerated),
                          static_cast<uint8_t>(value.type));
  TEST_ASSERT_EQUAL_UINT32(3, value.unsignedValue);
}

void test_bacnet_client_parses_av_present_value_real_ack() {
  const uint8_t response[] = {
    0x81,
    0x0A,
    0x00,
    0x17,
    0x01,
    0x00,
    0x30,
    0x07,
    0x0C,
    0x0C,
    0x00,
    0x80,
    0x00,
    0xC8,
    0x19,
    0x55,
    0x3E,
    0x44,
    0x41,
    0x48,
    0x00,
    0x00,
    0x3F,
  };
  const BacnetPropertyRequest propertyRequest{
    BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::AnalogValue),
                   200},
    BacnetPropertyId::PresentValue};
  BacnetValue value;

  TEST_ASSERT_TRUE(BacnetClient::parseReadPropertyAck(
    response, sizeof(response), 7, propertyRequest, value));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetValueType::Real),
                          static_cast<uint8_t>(value.type));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.5f, value.realValue);
  TEST_ASSERT_EQUAL_STRING("12.500", value.displayText());
}

void test_bacnet_client_parses_ai_present_value_ack_with_type_zero() {
  const uint8_t response[] = {
    0x81,
    0x0A,
    0x00,
    0x17,
    0x01,
    0x00,
    0x30,
    0x09,
    0x0C,
    0x0C,
    0x00,
    0x00,
    0x00,
    0x64,
    0x19,
    0x55,
    0x3E,
    0x44,
    0x41,
    0x20,
    0x00,
    0x00,
    0x3F,
  };
  const BacnetPropertyRequest propertyRequest{
    BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::AnalogInput),
                   100},
    BacnetPropertyId::PresentValue};
  BacnetValue value;

  TEST_ASSERT_TRUE(BacnetClient::parseReadPropertyAck(
    response, sizeof(response), 9, propertyRequest, value));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetValueType::Real),
                          static_cast<uint8_t>(value.type));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, value.realValue);
  TEST_ASSERT_EQUAL_STRING("10.000", value.displayText());
}

void test_bacnet_client_parses_status_flags_bit_string_ack() {
  const uint8_t response[] = {
    0x81,
    0x0A,
    0x00,
    0x15,
    0x01,
    0x00,
    0x30,
    0x08,
    0x0C,
    0x0C,
    0x00,
    0x80,
    0x00,
    0xC8,
    0x19,
    0x6F,
    0x3E,
    0x82,
    0x04,
    0x50,
    0x3F,
  };
  const BacnetPropertyRequest propertyRequest{
    BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::AnalogValue),
                   200},
    BacnetPropertyId::StatusFlags};
  BacnetValue value;

  TEST_ASSERT_TRUE(BacnetClient::parseReadPropertyAck(
    response, sizeof(response), 8, propertyRequest, value));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetValueType::BitString),
                          static_cast<uint8_t>(value.type));
  TEST_ASSERT_EQUAL_UINT8(4, value.bitStringBitCount);
  TEST_ASSERT_EQUAL_UINT32((1UL << 1) | (1UL << 3), value.bitStringValue);

  BacnetStatusFlags flags;
  TEST_ASSERT_TRUE(bacnetDecodeStatusFlags(value, flags));
  TEST_ASSERT_FALSE(flags.inAlarm);
  TEST_ASSERT_TRUE(flags.fault);
  TEST_ASSERT_FALSE(flags.overridden);
  TEST_ASSERT_TRUE(flags.outOfService);
}

void test_bacnet_object_health_state_derivation() {
  BacnetObjectStatus status;
  status.presentValueStatus = BacnetPropertyReadStatus::Ack;
  status.statusFlagsStatus = BacnetPropertyReadStatus::Ack;
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetObjectHealthState::Normal),
    static_cast<uint8_t>(bacnetDeriveObjectHealthState(status)));

  status.statusFlags.inAlarm = true;
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetObjectHealthState::Warning),
    static_cast<uint8_t>(bacnetDeriveObjectHealthState(status)));

  status.statusFlags.inAlarm = false;
  status.reliabilityStatus = BacnetPropertyReadStatus::Ack;
  status.reliability = 7;
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetObjectHealthState::Error),
    static_cast<uint8_t>(bacnetDeriveObjectHealthState(status)));

  status.reliability = 0;
  status.outOfServiceStatus = BacnetPropertyReadStatus::Ack;
  status.outOfService = true;
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetObjectHealthState::OutOfService),
    static_cast<uint8_t>(bacnetDeriveObjectHealthState(status)));

  BacnetObjectStatus unknown;
  unknown.presentValueStatus = BacnetPropertyReadStatus::Ack;
  unknown.statusFlagsStatus = BacnetPropertyReadStatus::UnsupportedProperty;
  unknown.eventStateStatus = BacnetPropertyReadStatus::UnsupportedProperty;
  unknown.reliabilityStatus = BacnetPropertyReadStatus::UnsupportedProperty;
  unknown.outOfServiceStatus = BacnetPropertyReadStatus::UnsupportedProperty;
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetObjectHealthState::Unknown),
    static_cast<uint8_t>(bacnetDeriveObjectHealthState(unknown)));

  BacnetObjectStatus failedPresentValue;
  failedPresentValue.presentValueStatus = BacnetPropertyReadStatus::Timeout;
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetObjectHealthState::Error),
    static_cast<uint8_t>(bacnetDeriveObjectHealthState(failedPresentValue)));
}

void test_bacnet_client_parses_object_list_entry_ack() {
  const uint8_t response[] = {
    0x81,
    0x0A,
    0x00,
    0x19,
    0x01,
    0x00,
    0x30,
    0x03,
    0x0C,
    0x0C,
    0x02,
    0x00,
    0x04,
    0xD2,
    0x19,
    0x4C,
    0x29,
    0x02,
    0x3E,
    0xC4,
    0x04,
    0xC0,
    0x00,
    0x0B,
    0x3F,
  };
  BacnetValue value;

  TEST_ASSERT_TRUE(BacnetClient::parseReadPropertyAck(
    response, sizeof(response), 3, BacnetPropertyId::ObjectList, value));
  TEST_ASSERT_EQUAL_STRING("19,11", value.text);
  TEST_ASSERT_EQUAL_UINT32(5, value.textLength);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetValueType::ObjectIdentifier),
                          static_cast<uint8_t>(value.type));
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(BacnetObjectType::MultiStateValue),
                           value.objectValue.type);
  TEST_ASSERT_EQUAL_UINT32(11, value.objectValue.instance);
}

void test_bacnet_client_parses_object_list_ack() {
  const uint8_t response[] = {
    0x81,
    0x0A,
    0x00,
    0x21,
    0x01,
    0x00,
    0x30,
    0x05,
    0x0C,
    0x0C,
    0x02,
    0x00,
    0x04,
    0xD2,
    0x19,
    0x4C,
    0x3E,
    0xC4,
    0x02,
    0x00,
    0x04,
    0xD2,
    0xC4,
    0x04,
    0xC0,
    0x00,
    0x0B,
    0xC4,
    0x04,
    0xC0,
    0x00,
    0x0C,
    0x3F,
  };
  BacnetValue value;

  TEST_ASSERT_TRUE(BacnetClient::parseReadPropertyAck(
    response, sizeof(response), 5, BacnetPropertyId::ObjectList, value));
  TEST_ASSERT_EQUAL_STRING("8,1234;19,11;19,12", value.text);
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetValueType::ObjectIdentifierList),
    static_cast<uint8_t>(value.type));
}

void test_bacnet_client_parses_read_property_error() {
  const uint8_t response[] = {
    0x81,
    0x0A,
    0x00,
    0x0D,
    0x01,
    0x00,
    0x50,
    0x04,
    0x0C,
    0x91,
    0x02,
    0x91,
    0x20,
  };
  BacnetValue value;

  TEST_ASSERT_TRUE(BacnetProtocol::parseReadPropertyError(response,
                                                          sizeof(response),
                                                          4,
                                                          value));
  TEST_ASSERT_EQUAL_STRING("error class 2 code 32", value.text);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetValueType::Error),
                          static_cast<uint8_t>(value.type));
}

void test_bacnet_client_parses_read_property_error_codes() {
  const uint8_t response[] = {
    0x81,
    0x0A,
    0x00,
    0x0D,
    0x01,
    0x00,
    0x50,
    0x04,
    0x0C,
    0x91,
    0x02,
    0x91,
    0x20,
  };
  BacnetValue value;
  uint32_t errorClass = 0;
  uint32_t errorCode = 0;

  TEST_ASSERT_TRUE(BacnetClient::parseReadPropertyError(response,
                                                        sizeof(response),
                                                        4,
                                                        value,
                                                        &errorClass,
                                                        &errorCode));
  TEST_ASSERT_EQUAL_UINT32(2, errorClass);
  TEST_ASSERT_EQUAL_UINT32(32, errorCode);
}

void test_bacnet_client_parses_property_list_count_ack() {
  const uint8_t response[] = {
    0x81,
    0x0A,
    0x00,
    0x17,
    0x01,
    0x00,
    0x30,
    0x09,
    0x0C,
    0x02,
    0x00,
    0x04,
    0xD2,
    0x1A,
    0x01,
    0x73,
    0x29,
    0x00,
    0x3E,
    0x21,
    0x03,
    0x3F,
  };
  const BacnetPropertyRequest request{
    BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::Device), 1234},
    BacnetPropertyId::PropertyList,
    0};
  BacnetValue value;

  TEST_ASSERT_TRUE(BacnetClient::parseReadPropertyAck(
    response, sizeof(response), 9, request, value));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetValueType::Unsigned),
                          static_cast<uint8_t>(value.type));
  TEST_ASSERT_EQUAL_UINT32(3, value.unsignedValue);
}

void test_bacnet_client_parses_property_list_entry_ack() {
  const uint8_t response[] = {
    0x81,
    0x0A,
    0x00,
    0x17,
    0x01,
    0x00,
    0x30,
    0x0A,
    0x0C,
    0x02,
    0x00,
    0x04,
    0xD2,
    0x1A,
    0x01,
    0x73,
    0x29,
    0x01,
    0x3E,
    0x91,
    0x4D,
    0x3F,
  };
  const BacnetPropertyRequest request{
    BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::Device), 1234},
    BacnetPropertyId::PropertyList,
    1};
  BacnetValue value;

  TEST_ASSERT_TRUE(BacnetClient::parseReadPropertyAck(
    response, sizeof(response), 10, request, value));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetValueType::Enumerated),
                          static_cast<uint8_t>(value.type));
  TEST_ASSERT_EQUAL_UINT32(
    static_cast<uint32_t>(BacnetPropertyId::ObjectName), value.unsignedValue);
}

void test_bacnet_device_session_keeps_remote_device_metadata() {
  BacnetClient client;
  IPAddress address(192, 168, 1, 50);
  BacnetDeviceSession session(client, 1234, address, 47809);

  TEST_ASSERT_EQUAL_UINT32(1234, session.deviceInstance());
  TEST_ASSERT_EQUAL_STRING("192.168.1.50", session.address().toString().c_str());
  TEST_ASSERT_EQUAL_UINT16(47809, session.port());
  TEST_ASSERT_EQUAL_PTR(&client, &session.client());

  const BacnetObjectId deviceObject = session.deviceObject();
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(BacnetObjectType::Device),
                           deviceObject.type);
  TEST_ASSERT_EQUAL_UINT32(1234, deviceObject.instance);
}

void test_bacnet_device_session_from_endpoint_keeps_metadata() {
  BacnetClient client;
  IPAddress address(192, 168, 1, 51);
  BacnetDeviceSession session =
    BacnetDeviceSession::fromEndpoint(client, 5678, address, 47810);

  TEST_ASSERT_EQUAL_UINT32(5678, session.deviceInstance());
  TEST_ASSERT_EQUAL_STRING("192.168.1.51", session.address().toString().c_str());
  TEST_ASSERT_EQUAL_UINT16(47810, session.port());
  TEST_ASSERT_EQUAL_PTR(&client, &session.client());
}

void test_bacnet_device_session_from_i_am_uses_default_port() {
  BacnetClient client;
  BacnetIAmDevice device;
  device.address = IPAddress(192, 168, 1, 52);
  device.deviceInstance = 9012;
  device.vendorId = 222;

  BacnetDeviceSession session = BacnetDeviceSession::fromIAm(client, device);

  TEST_ASSERT_EQUAL_UINT32(9012, session.deviceInstance());
  TEST_ASSERT_EQUAL_STRING("192.168.1.52", session.address().toString().c_str());
  TEST_ASSERT_EQUAL_UINT16(BacnetClient::kDefaultPort, session.port());
  TEST_ASSERT_EQUAL_PTR(&client, &session.client());
}

void test_bacnet_device_session_reports_send_failure() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(0, 0, 0, 0));
  BacnetValue value;

  const BacnetDeviceSessionReadStatus status = session.readProperty(
    BacnetObjectType::Device, 1234, BacnetPropertyId::ObjectName, value, 0);

  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::SendFailed),
    static_cast<uint8_t>(status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetValueType::Empty),
                          static_cast<uint8_t>(value.type));
}

void test_bacnet_device_session_caches_failed_read_status() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(0, 0, 0, 0));
  const BacnetObjectId objectId{
    static_cast<uint16_t>(BacnetObjectType::Device), 1234};
  BacnetCachedProperty cached;
  BacnetValue value;

  TEST_ASSERT_EQUAL_UINT32(0, session.cachedPropertyCount());
  TEST_ASSERT_FALSE(session.cachedProperty(
    objectId, BacnetPropertyId::ObjectName, cached));

  const BacnetDeviceSessionReadStatus status = session.readProperty(
    objectId, BacnetPropertyId::ObjectName, value, 0);

  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::SendFailed),
    static_cast<uint8_t>(status));
  TEST_ASSERT_TRUE(session.cachedProperty(
    objectId, BacnetPropertyId::ObjectName, cached));
  TEST_ASSERT_EQUAL_UINT32(1, session.cachedPropertyCount());
  TEST_ASSERT_EQUAL_UINT16(objectId.type, cached.request.object.type);
  TEST_ASSERT_EQUAL_UINT32(objectId.instance, cached.request.object.instance);
  TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(BacnetPropertyId::ObjectName),
                           static_cast<uint32_t>(cached.request.property));
  TEST_ASSERT_EQUAL_UINT32(kBacnetNoArrayIndex, cached.request.arrayIndex);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetPropertyReadStatus::SendFailed),
                          static_cast<uint8_t>(cached.status));
  TEST_ASSERT_FALSE(cached.hasValue);
}

void test_bacnet_property_exposes_cached_read_state() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(0, 0, 0, 0));
  const BacnetProperty property = session.object(BacnetObjectType::Device, 1234)
                                    .property(BacnetPropertyId::ObjectName);
  BacnetValue value;

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetValueType::Empty),
                          static_cast<uint8_t>(property.lastValue().type));
  TEST_ASSERT_FALSE(property.hasValue());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetPropertyReadStatus::Skipped),
                          static_cast<uint8_t>(property.lastStatus()));
  TEST_ASSERT_EQUAL_UINT32(0, property.lastUpdateMs());
  TEST_ASSERT_EQUAL_UINT32(0, property.lastAttemptMs());
  TEST_ASSERT_EQUAL_UINT32(0, property.lastSuccessMs());

  const BacnetDeviceSessionReadStatus status = property.read(value, 0);

  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::SendFailed),
    static_cast<uint8_t>(status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetValueType::Empty),
                          static_cast<uint8_t>(property.lastValue().type));
  TEST_ASSERT_FALSE(property.hasValue());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetPropertyReadStatus::SendFailed),
                          static_cast<uint8_t>(property.lastStatus()));
  TEST_ASSERT_EQUAL_UINT32(0, property.lastUpdateMs());
  TEST_ASSERT_EQUAL_UINT32(0, property.lastSuccessMs());
}

#if defined(UNIT_TEST)
void test_bacnet_property_cache_keeps_value_after_failed_refresh() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(192, 0, 2, 50));
  const BacnetObjectId objectId{
    static_cast<uint16_t>(BacnetObjectType::AnalogValue), 200};
  const BacnetPropertyRequest request{
    objectId, BacnetPropertyId::PresentValue, kBacnetNoArrayIndex};
  BacnetValue value;
  value.type = BacnetValueType::Real;
  value.realValue = 12.5f;
  snprintf(value.text, sizeof(value.text), "12.5");
  value.textLength = strlen(value.text);

  session.updatePropertyCache(request, BacnetPropertyReadStatus::Ack, &value, 100);

  const BacnetProcessObject analogValue = session.analogValue(200);
  TEST_ASSERT_TRUE(analogValue.hasPresentValue());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetPropertyReadStatus::Ack),
                          static_cast<uint8_t>(analogValue.presentValueStatus()));
  TEST_ASSERT_EQUAL_UINT32(100, analogValue.presentValueLastAttemptMs());
  TEST_ASSERT_EQUAL_UINT32(100, analogValue.presentValueLastSuccessMs());
  TEST_ASSERT_EQUAL_STRING("12.5", analogValue.presentValue().displayText());

  session.updatePropertyCache(
    request, BacnetPropertyReadStatus::Timeout, nullptr, 200);

  TEST_ASSERT_TRUE(analogValue.hasPresentValue());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetPropertyReadStatus::Timeout),
                          static_cast<uint8_t>(analogValue.presentValueStatus()));
  TEST_ASSERT_EQUAL_UINT32(200, analogValue.presentValueLastAttemptMs());
  TEST_ASSERT_EQUAL_UINT32(100, analogValue.presentValueLastSuccessMs());
  TEST_ASSERT_EQUAL_STRING("12.5", analogValue.presentValue().displayText());

  BacnetValue badValue;
  badValue.type = BacnetValueType::CharacterString;
  snprintf(badValue.text, sizeof(badValue.text), "decode-error");
  badValue.textLength = strlen(badValue.text);
  session.updatePropertyCache(
    request, BacnetPropertyReadStatus::DecodeError, &badValue, 300);

  TEST_ASSERT_TRUE(analogValue.hasPresentValue());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetPropertyReadStatus::DecodeError),
                          static_cast<uint8_t>(analogValue.presentValueStatus()));
  TEST_ASSERT_EQUAL_UINT32(300, analogValue.presentValueLastAttemptMs());
  TEST_ASSERT_EQUAL_UINT32(100, analogValue.presentValueLastSuccessMs());
  TEST_ASSERT_EQUAL_STRING("12.5", analogValue.presentValue().displayText());
}
#endif

void test_bacnet_device_session_creates_remote_object() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(192, 168, 1, 50));

  const BacnetRemoteObject object =
    session.object(BacnetObjectType::MultiStateValue, 2000);
  const BacnetObjectId objectId = object.objectId();

  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(BacnetObjectType::MultiStateValue),
                           objectId.type);
  TEST_ASSERT_EQUAL_UINT32(2000, objectId.instance);
}

void test_bacnet_device_session_creates_process_object_helpers() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(192, 168, 1, 50));

  const BacnetProcessObject ai = session.analogInput(100);
  const BacnetProcessObject ao = session.analogOutput(101);
  const BacnetProcessObject av = session.analogValue(200);
  const BacnetProcessObject bi = session.binaryInput(300);
  const BacnetProcessObject bo = session.binaryOutput(301);
  const BacnetProcessObject bv = session.binaryValue(302);
  const BacnetProcessObject msi = session.multiStateInput(400);
  const BacnetProcessObject mso = session.multiStateOutput(401);
  const BacnetProcessObject msv = session.multiStateValue(402);

  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(BacnetObjectType::AnalogInput),
                           ai.objectId().type);
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(BacnetObjectType::AnalogOutput),
                           ao.objectId().type);
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(BacnetObjectType::AnalogValue),
                           av.objectId().type);
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(BacnetObjectType::BinaryInput),
                           bi.objectId().type);
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(BacnetObjectType::BinaryOutput),
                           bo.objectId().type);
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(BacnetObjectType::BinaryValue),
                           bv.objectId().type);
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(BacnetObjectType::MultiStateInput),
                           msi.objectId().type);
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(BacnetObjectType::MultiStateOutput),
                           mso.objectId().type);
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(BacnetObjectType::MultiStateValue),
                           msv.objectId().type);
  TEST_ASSERT_EQUAL_UINT32(200, av.objectId().instance);
}

void test_bacnet_remote_object_creates_property_request() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(192, 168, 1, 50));
  const BacnetRemoteObject object =
    session.object(BacnetObjectId{
      static_cast<uint16_t>(BacnetObjectType::MultiStateValue), 2000});

  const BacnetProperty property =
    object.property(BacnetPropertyId::StateText, 2);
  const BacnetPropertyRequest request = property.request();

  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(BacnetObjectType::MultiStateValue),
                           property.objectId().type);
  TEST_ASSERT_EQUAL_UINT32(2000, property.objectId().instance);
  TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(BacnetPropertyId::StateText),
                           static_cast<uint32_t>(property.propertyId()));
  TEST_ASSERT_EQUAL_UINT32(2, property.arrayIndex());
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(BacnetObjectType::MultiStateValue),
                           request.object.type);
  TEST_ASSERT_EQUAL_UINT32(2000, request.object.instance);
  TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(BacnetPropertyId::StateText),
                           static_cast<uint32_t>(request.property));
  TEST_ASSERT_EQUAL_UINT32(2, request.arrayIndex);
}

void test_bacnet_remote_object_read_reports_send_failure() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(0, 0, 0, 0));
  const BacnetRemoteObject object =
    session.object(BacnetObjectType::AnalogValue, 200);
  BacnetValue value;

  const BacnetDeviceSessionReadStatus status =
    object.readPresentValue(value, 0);

  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::SendFailed),
    static_cast<uint8_t>(status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetValueType::Empty),
                          static_cast<uint8_t>(value.type));
}

void test_bacnet_process_object_present_value_uses_generic_property_api() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(0, 0, 0, 0));
  const BacnetProcessObject analogValue = session.analogValue(200);
  const BacnetProperty presentValue = analogValue.presentValueProperty();
  const BacnetPropertyRequest request = presentValue.request();

  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(BacnetObjectType::AnalogValue),
                           request.object.type);
  TEST_ASSERT_EQUAL_UINT32(200, request.object.instance);
  TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(BacnetPropertyId::PresentValue),
                           static_cast<uint32_t>(request.property));
  TEST_ASSERT_EQUAL_UINT32(kBacnetNoArrayIndex, request.arrayIndex);

  const BacnetDeviceSessionReadStatus status = analogValue.readPresentValue(0);

  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::SendFailed),
    static_cast<uint8_t>(status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetPropertyReadStatus::SendFailed),
                          static_cast<uint8_t>(analogValue.presentValueStatus()));
  TEST_ASSERT_FALSE(analogValue.hasPresentValue());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetValueType::Empty),
                          static_cast<uint8_t>(analogValue.presentValue().type));
  TEST_ASSERT_EQUAL_UINT32(0, analogValue.presentValueLastUpdateMs());
  TEST_ASSERT_EQUAL_UINT32(0, analogValue.presentValueLastSuccessMs());
}

void test_bacnet_process_object_metadata_uses_generic_property_api() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(0, 0, 0, 0));
  const BacnetProcessObject analogValue = session.analogValue(200);
  BacnetValue value;

  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::SendFailed),
    static_cast<uint8_t>(analogValue.readEngineeringUnits(value, 0)));
  TEST_ASSERT_EQUAL_UINT32(
    static_cast<uint32_t>(BacnetPropertyId::Units),
    static_cast<uint32_t>(
      analogValue.property(BacnetPropertyId::Units).request().property));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetValueType::Empty),
                          static_cast<uint8_t>(analogValue.engineeringUnits().type));
  TEST_ASSERT_NULL(analogValue.engineeringUnitSymbol());

  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::SendFailed),
    static_cast<uint8_t>(analogValue.readMinPresentValue(value, 0)));
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::SendFailed),
    static_cast<uint8_t>(analogValue.readMaxPresentValue(value, 0)));
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::SendFailed),
    static_cast<uint8_t>(analogValue.readResolution(value, 0)));
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::SendFailed),
    static_cast<uint8_t>(analogValue.readCovIncrement(value, 0)));
}

void test_bacnet_process_object_status_uses_generic_status_reads() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(0, 0, 0, 0));
  const BacnetProcessObject analogValue = session.analogValue(200);

  const BacnetObjectStatus status = analogValue.readStatus(0);

  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(BacnetObjectType::AnalogValue),
                           status.objectId.type);
  TEST_ASSERT_EQUAL_UINT32(200, status.objectId.instance);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetPropertyReadStatus::SendFailed),
                          static_cast<uint8_t>(status.presentValueStatus));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetPropertyReadStatus::SendFailed),
                          static_cast<uint8_t>(analogValue.statusFlagsStatus()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetPropertyReadStatus::SendFailed),
                          static_cast<uint8_t>(analogValue.eventStateStatus()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetPropertyReadStatus::SendFailed),
                          static_cast<uint8_t>(analogValue.reliabilityStatus()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetPropertyReadStatus::SendFailed),
                          static_cast<uint8_t>(analogValue.outOfServiceStatus()));
  TEST_ASSERT_FALSE(analogValue.outOfService());
}

void test_bacnet_property_read_reports_send_failure() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(0, 0, 0, 0));
  const BacnetProperty property =
    session.object(BacnetObjectType::Device, 1234)
      .property(BacnetPropertyId::ObjectName);
  BacnetValue value;

  const BacnetDeviceSessionReadStatus status = property.read(value, 0);

  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::SendFailed),
    static_cast<uint8_t>(status));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetValueType::Empty),
                          static_cast<uint8_t>(value.type));
}

void test_bacnet_remote_object_property_list_reports_send_failure() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(0, 0, 0, 0));
  const BacnetRemoteObject object =
    session.object(BacnetObjectType::AnalogValue, 200);
  BacnetPropertyId properties[4] = {};

  const BacnetPropertyListReadResult result =
    object.readPropertyList(properties, 4, 0);

  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetPropertyReadStatus::SendFailed),
    static_cast<uint8_t>(result.status));
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(result.advertised));
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(result.stored));
}

void test_bacnet_remote_object_read_all_attempts_each_property() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(0, 0, 0, 0));
  const BacnetRemoteObject object =
    session.object(BacnetObjectType::AnalogValue, 200);
  const BacnetPropertyId properties[] = {
    BacnetPropertyId::ObjectName,
    BacnetPropertyId::PresentValue,
  };
  BacnetPropertyReadResult results[2] = {};

  const BacnetPropertyReadAllResult summary = object.readAllProperties(
    properties, 2, results, 2, 0);

  TEST_ASSERT_EQUAL_UINT32(2, static_cast<uint32_t>(summary.requested));
  TEST_ASSERT_EQUAL_UINT32(2, static_cast<uint32_t>(summary.attempted));
  TEST_ASSERT_EQUAL_UINT32(2, static_cast<uint32_t>(summary.stored));
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(summary.acked));
  TEST_ASSERT_EQUAL_UINT32(2, static_cast<uint32_t>(summary.failed));
  TEST_ASSERT_FALSE(summary.truncated);
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetPropertyReadStatus::SendFailed),
    static_cast<uint8_t>(results[0].status));
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetPropertyReadStatus::SendFailed),
    static_cast<uint8_t>(results[1].status));

  BacnetCachedProperty cached;
  TEST_ASSERT_TRUE(session.cachedProperty(
    object.objectId(), BacnetPropertyId::ObjectName, cached));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetPropertyReadStatus::SendFailed),
                          static_cast<uint8_t>(cached.status));
  TEST_ASSERT_TRUE(session.cachedProperty(
    object.objectId(), BacnetPropertyId::PresentValue, cached));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetPropertyReadStatus::SendFailed),
                          static_cast<uint8_t>(cached.status));
}

void test_bacnet_remote_object_read_all_advertised_reports_property_list_failure() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(0, 0, 0, 0));
  const BacnetRemoteObject object =
    session.object(BacnetObjectType::AnalogValue, 200);
  BacnetPropertyId properties[4] = {};
  BacnetPropertyReadResult results[4] = {};

  const BacnetPropertyReadAllResult summary = object.readAllAdvertisedProperties(
    properties, 4, results, 4, 0);

  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetPropertyReadStatus::SendFailed),
    static_cast<uint8_t>(summary.propertyListStatus));
  TEST_ASSERT_EQUAL_UINT32(0, summary.advertised);
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(summary.collected));
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(summary.requested));
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(summary.attempted));
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(summary.stored));

  BacnetCachedProperty cached;
  TEST_ASSERT_TRUE(session.cachedProperty(
    object.objectId(), BacnetPropertyId::PropertyList, cached, 0));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetPropertyReadStatus::SendFailed),
                          static_cast<uint8_t>(cached.status));
}

void test_bacnet_object_scan_options_defaults() {
  BacnetObjectScanOptions options;

  TEST_ASSERT_NULL(options.objectTypes);
  TEST_ASSERT_EQUAL_UINT32(0, options.objectTypeCount);
  TEST_ASSERT_EQUAL_UINT32(600, options.maxObjectListEntries);
  TEST_ASSERT_EQUAL_UINT32(BacnetDeviceSession::kDefaultReadTimeoutMs,
                           options.readTimeoutMs);
  TEST_ASSERT_TRUE(options.readObjectName);
  TEST_ASSERT_TRUE(options.readDescription);
  TEST_ASSERT_TRUE(options.readPresentValue);
  TEST_ASSERT_TRUE(options.acceptsObjectType(BacnetObjectType::Device));
  TEST_ASSERT_TRUE(options.acceptsObjectType(
    BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::AnalogValue),
                   200}));
}

void test_bacnet_object_scan_options_filter_object_types() {
  const BacnetObjectType objectTypes[] = {BacnetObjectType::AnalogValue,
                                          BacnetObjectType::MultiStateValue};
  BacnetObjectScanOptions options;
  options.objectTypes = objectTypes;
  options.objectTypeCount = 2;

  TEST_ASSERT_TRUE(options.acceptsObjectType(BacnetObjectType::AnalogValue));
  TEST_ASSERT_TRUE(options.acceptsObjectType(
    BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::MultiStateValue),
                   2000}));
  TEST_ASSERT_FALSE(options.acceptsObjectType(BacnetObjectType::Device));
}

void test_bacnet_object_scan_options_accepts_analog_input_type_zero() {
  BacnetObjectScanOptions options;
  const BacnetObjectType objectTypes[] = {BacnetObjectType::AnalogInput};
  bacnetSetObjectTypeFilter(options, objectTypes);

  TEST_ASSERT_TRUE(options.acceptsObjectType(BacnetObjectType::AnalogInput));
  TEST_ASSERT_TRUE(options.acceptsObjectType(
    BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::AnalogInput),
                   100}));
  TEST_ASSERT_FALSE(options.acceptsObjectType(BacnetObjectType::AnalogOutput));
}

void test_bacnet_scanned_object_defaults_are_skipped() {
  BacnetScannedObject scanned;
  BacnetObjectScanResult result;

  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::Skipped),
    static_cast<uint8_t>(scanned.objectNameStatus));
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::Skipped),
    static_cast<uint8_t>(scanned.descriptionStatus));
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::Skipped),
    static_cast<uint8_t>(scanned.presentValueStatus));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetValueType::Empty),
                          static_cast<uint8_t>(scanned.objectName.type));
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::Skipped),
    static_cast<uint8_t>(result.objectListCountStatus));
  TEST_ASSERT_EQUAL_UINT32(0, result.objectListCount);
  TEST_ASSERT_EQUAL_UINT32(0, result.inspected);
  TEST_ASSERT_EQUAL_UINT32(0, result.found);
  TEST_ASSERT_EQUAL_UINT32(0, result.stored);
  TEST_ASSERT_FALSE(result.truncated);
}

void test_bacnet_device_session_scan_object_list_invalid_target() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(0, 0, 0, 0));
  BacnetObjectScanOptions options;
  options.readTimeoutMs = 0;
  BacnetScannedObject results[1];
  results[0].objectId = BacnetObjectId{
    static_cast<uint16_t>(BacnetObjectType::AnalogValue), 42};

  const BacnetObjectScanResult result =
    session.scanObjectList(options, results, 1);

  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::SendFailed),
    static_cast<uint8_t>(result.objectListCountStatus));
  TEST_ASSERT_EQUAL_UINT32(0, result.objectListCount);
  TEST_ASSERT_EQUAL_UINT32(0, result.inspected);
  TEST_ASSERT_EQUAL_UINT32(0, result.found);
  TEST_ASSERT_EQUAL_UINT32(0, result.stored);
  TEST_ASSERT_FALSE(result.truncated);
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(BacnetObjectType::AnalogValue),
                           results[0].objectId.type);
  TEST_ASSERT_EQUAL_UINT32(42, results[0].objectId.instance);

  const BacnetObjectScanResult zeroCapacityResult =
    session.scanObjectList(options, nullptr, 0);
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::SendFailed),
    static_cast<uint8_t>(zeroCapacityResult.objectListCountStatus));
  TEST_ASSERT_EQUAL_UINT32(0, zeroCapacityResult.stored);
}

void test_bacnet_object_list_scan_job_initial_state() {
  BacnetObjectListScanJob job;

  TEST_ASSERT_TRUE(job.isIdle());
  TEST_ASSERT_FALSE(job.isActive());
  TEST_ASSERT_FALSE(job.isComplete());
  TEST_ASSERT_FALSE(job.isFailed());
  TEST_ASSERT_FALSE(job.isCancelled());
  TEST_ASSERT_FALSE(job.isTerminal());
  TEST_ASSERT_FALSE(job.requestInFlight());
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetObjectListScanJobStatus::Idle),
    static_cast<uint8_t>(job.status()));
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetObjectListScanPhase::Idle),
    static_cast<uint8_t>(job.phase()));
  TEST_ASSERT_EQUAL_STRING("idle",
                           bacnetObjectListScanJobStatusText(job.status()));
  TEST_ASSERT_EQUAL_STRING("idle", bacnetObjectListScanPhaseText(job.phase()));
  TEST_ASSERT_EQUAL_UINT32(0, job.currentIndex());
  TEST_ASSERT_EQUAL_UINT32(0, job.summary().stored);

  const BacnetObjectListScanProgress progress = job.progress();
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetObjectListScanJobStatus::Idle),
    static_cast<uint8_t>(progress.status));
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetObjectListScanPhase::Idle),
    static_cast<uint8_t>(progress.phase));
  TEST_ASSERT_FALSE(progress.requestInFlight);
  TEST_ASSERT_EQUAL_UINT32(0, progress.currentIndex);
}

void test_bacnet_object_list_scan_job_reports_send_failure() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(0, 0, 0, 0));
  BacnetObjectListScanJob job;
  BacnetObjectScanOptions options;
  options.readTimeoutMs = 0;
  BacnetScannedObject results[1];

  const bool started =
    session.beginObjectListScan(job, options, results, 1, 1000);

  TEST_ASSERT_FALSE(started);
  TEST_ASSERT_TRUE(job.isFailed());
  TEST_ASSERT_TRUE(job.isTerminal());
  TEST_ASSERT_FALSE(job.requestInFlight());
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetObjectListScanPhase::Failed),
    static_cast<uint8_t>(job.phase()));
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::SendFailed),
    static_cast<uint8_t>(job.summary().objectListCountStatus));
  TEST_ASSERT_EQUAL_UINT32(0, job.summary().stored);
}

void test_bacnet_object_list_scan_job_busy_protects_blocking_read() {
  BacnetClient client;
  TEST_ASSERT_TRUE(client.begin(47813));
  BacnetDeviceSession session(client, 1234, IPAddress(192, 0, 2, 50));
  BacnetObjectListScanJob job;
  BacnetObjectScanOptions options;
  options.readTimeoutMs = 1000;
  BacnetScannedObject results[1];

  const bool started =
    session.beginObjectListScan(job, options, results, 1, 1000);
  if (!started || !job.requestInFlight()) {
    TEST_IGNORE_MESSAGE("UDP send did not enter object-list scan in-flight state");
  }

  BacnetValue value;
  const auto status =
    session.readProperty(BacnetObjectType::AnalogValue, 200, BacnetPropertyId::PresentValue, value, 1);

  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::Busy),
    static_cast<uint8_t>(status));
  session.cancelObjectListScan(job);
  TEST_ASSERT_TRUE(job.isCancelled());
}

void test_bacnet_server_lifecycle() {
  BacnetServer server;

  TEST_ASSERT_FALSE(server.isRunning());
  TEST_ASSERT_TRUE(server.begin(1234));
  TEST_ASSERT_TRUE(server.isRunning());
  TEST_ASSERT_EQUAL_UINT32(1234, server.deviceInstance());
  TEST_ASSERT_EQUAL_UINT16(BacnetServer::kDefaultPort, server.port());

  server.end();
  TEST_ASSERT_FALSE(server.isRunning());
}

void test_bacnet_subscribe_options_defaults() {
  BacnetSubscribeOptions options;

  TEST_ASSERT_EQUAL_UINT32(10000, options.fallbackPollMs);
  TEST_ASSERT_EQUAL_UINT32(BacnetDeviceSession::kDefaultReadTimeoutMs,
                           options.timeoutMs);
  TEST_ASSERT_TRUE(options.immediateFirstRead);
  TEST_ASSERT_TRUE(options.notifyOnStatusChange);
}

void test_bacnet_property_subscription_is_move_only() {
  static_assert(!std::is_copy_constructible<BacnetPropertySubscription>::value,
                "BacnetPropertySubscription must not be copy-constructible");
  static_assert(!std::is_copy_assignable<BacnetPropertySubscription>::value,
                "BacnetPropertySubscription must not be copy-assignable");
  static_assert(std::is_move_constructible<BacnetPropertySubscription>::value,
                "BacnetPropertySubscription should be move-constructible");
  static_assert(std::is_move_assignable<BacnetPropertySubscription>::value,
                "BacnetPropertySubscription should be move-assignable");
  TEST_ASSERT_TRUE(true);
}

void test_bacnet_property_subscribe_exposes_identity_and_defaults() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(192, 168, 1, 50));
  BacnetProperty property =
    session.object(BacnetObjectType::AnalogValue, 200)
      .property(BacnetPropertyId::PresentValue, 1);

  BacnetPropertySubscription subscription =
    property.subscribe(nullptr, nullptr, BacnetSubscribeOptions{});

  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(BacnetObjectType::AnalogValue),
                           subscription.objectId().type);
  TEST_ASSERT_EQUAL_UINT32(200, subscription.objectId().instance);
  TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(BacnetPropertyId::PresentValue),
                           static_cast<uint32_t>(subscription.propertyId()));
  TEST_ASSERT_EQUAL_UINT32(1, subscription.arrayIndex());
  TEST_ASSERT_TRUE(subscription.active());
  TEST_ASSERT_FALSE(subscription.inFlight());
  TEST_ASSERT_FALSE(subscription.hasValue());
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::Skipped),
    static_cast<uint8_t>(subscription.lastStatus()));
  TEST_ASSERT_EQUAL_UINT32(0, subscription.lastUpdateMs());
}

void test_bacnet_subscription_request_refresh_and_fallback_zero_behavior() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(192, 168, 1, 50));
  BacnetProperty property =
    session.object(BacnetObjectType::AnalogValue, 200)
      .property(BacnetPropertyId::PresentValue);

  SubscriptionCallbackCapture capture;
  BacnetSubscribeOptions options;
  options.fallbackPollMs = 0;
  options.immediateFirstRead = false;

  BacnetPropertySubscription subscription =
    property.subscribe(captureSubscriptionNotification, &capture, options);

  session.poll(subscription, 1000);
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(capture.calls));
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::Skipped),
    static_cast<uint8_t>(subscription.lastStatus()));

  subscription.requestRefresh();
  session.poll(subscription, 1001);
  TEST_ASSERT_EQUAL_UINT32(1, static_cast<uint32_t>(capture.calls));
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::SendFailed),
    static_cast<uint8_t>(subscription.lastStatus()));
  TEST_ASSERT_EQUAL_UINT32(1001, subscription.lastUpdateMs());
  TEST_ASSERT_FALSE(subscription.hasValue());
  TEST_ASSERT_TRUE(capture.statusChanged);
  TEST_ASSERT_TRUE(bacnetSubscriptionReasonContains(
    capture.reason, BacnetSubscriptionNotificationReason::StatusChanged));
}

void test_bacnet_subscription_stop_disables_polling() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(192, 168, 1, 50));
  BacnetProperty property =
    session.object(BacnetObjectType::MultiStateValue, 2000)
      .property(BacnetPropertyId::PresentValue);

  SubscriptionCallbackCapture capture;
  BacnetPropertySubscription subscription =
    property.subscribe(captureSubscriptionNotification, &capture);

  subscription.stop();
  session.poll(subscription, 2000);

  TEST_ASSERT_FALSE(subscription.active());
  TEST_ASSERT_FALSE(subscription.inFlight());
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(capture.calls));
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::Skipped),
    static_cast<uint8_t>(subscription.lastStatus()));
}

void test_bacnet_subscription_notify_status_change_false_suppresses_callback() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(192, 168, 1, 50));
  BacnetProperty property =
    session.object(BacnetObjectType::AnalogValue, 200)
      .property(BacnetPropertyId::PresentValue);

  SubscriptionCallbackCapture capture;
  BacnetSubscribeOptions options;
  options.fallbackPollMs = 0;
  options.immediateFirstRead = false;
  options.notifyOnStatusChange = false;

  BacnetPropertySubscription subscription =
    property.subscribe(captureSubscriptionNotification, &capture, options);

  subscription.requestRefresh();
  session.poll(subscription, 1000);

  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::SendFailed),
    static_cast<uint8_t>(subscription.lastStatus()));
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(capture.calls));
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetSubscriptionNotificationReason::None),
    static_cast<uint8_t>(subscription.lastNotificationReason()));
}

void test_bacnet_subscription_immediate_first_read_works_with_fallback_zero() {
  BacnetClient client;
  BacnetDeviceSession session(client, 1234, IPAddress(192, 168, 1, 50));
  BacnetProperty property =
    session.object(BacnetObjectType::AnalogValue, 200)
      .property(BacnetPropertyId::PresentValue);

  SubscriptionCallbackCapture capture;
  BacnetSubscribeOptions options;
  options.fallbackPollMs = 0;
  options.immediateFirstRead = true;

  BacnetPropertySubscription subscription =
    property.subscribe(captureSubscriptionNotification, &capture, options);

  session.poll(subscription, 1000);
  TEST_ASSERT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::SendFailed),
    static_cast<uint8_t>(subscription.lastStatus()));
  TEST_ASSERT_EQUAL_UINT32(1000, subscription.lastUpdateMs());

  session.poll(subscription, 2000);
  TEST_ASSERT_EQUAL_UINT32(1000, subscription.lastUpdateMs());
}

void test_bacnet_subscription_move_while_in_flight_keeps_session_reference_safe() {
  BacnetClient client;
  TEST_ASSERT_TRUE(client.begin(47810));
  BacnetDeviceSession session(client, 1234, IPAddress(192, 0, 2, 50));
  BacnetProperty property =
    session.object(BacnetObjectType::AnalogValue, 200)
      .property(BacnetPropertyId::PresentValue);

  BacnetPropertySubscription subscription = property.subscribe(nullptr);
  session.poll(subscription, 1000);

  if (!subscription.inFlight()) {
    TEST_IGNORE_MESSAGE("UDP send did not enter in-flight state");
  }

  BacnetPropertySubscription moved(std::move(subscription));
  TEST_ASSERT_FALSE(subscription.active());
  TEST_ASSERT_FALSE(subscription.inFlight());
  TEST_ASSERT_TRUE(moved.active());
  TEST_ASSERT_TRUE(moved.inFlight());

  BacnetValue value;
  const auto status =
    session.readProperty(BacnetObjectType::AnalogValue, 200, BacnetPropertyId::PresentValue, value, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetDeviceSessionReadStatus::Busy),
                          static_cast<uint8_t>(status));
}

void test_bacnet_subscription_stop_while_in_flight_allows_blocking_read() {
  BacnetClient client;
  TEST_ASSERT_TRUE(client.begin(47811));
  BacnetDeviceSession session(client, 1234, IPAddress(192, 0, 2, 50));
  BacnetProperty property =
    session.object(BacnetObjectType::AnalogValue, 200)
      .property(BacnetPropertyId::PresentValue);

  BacnetPropertySubscription subscription = property.subscribe(nullptr);
  session.poll(subscription, 1000);

  if (!subscription.inFlight()) {
    TEST_IGNORE_MESSAGE("UDP send did not enter in-flight state");
  }

  subscription.stop();
  TEST_ASSERT_FALSE(subscription.active());
  TEST_ASSERT_FALSE(subscription.inFlight());

  BacnetValue value;
  const auto status =
    session.readProperty(BacnetObjectType::AnalogValue, 200, BacnetPropertyId::PresentValue, value, 1);
  TEST_ASSERT_NOT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::Busy),
    static_cast<uint8_t>(status));
}

void test_bacnet_subscription_destruction_while_in_flight_releases_session() {
  BacnetClient client;
  TEST_ASSERT_TRUE(client.begin(47812));
  BacnetDeviceSession session(client, 1234, IPAddress(192, 0, 2, 50));
  bool enteredInFlight = false;

  {
    BacnetProperty property =
      session.object(BacnetObjectType::AnalogValue, 200)
        .property(BacnetPropertyId::PresentValue);
    BacnetPropertySubscription subscription = property.subscribe(nullptr);
    session.poll(subscription, 1000);
    enteredInFlight = subscription.inFlight();
  }

  if (!enteredInFlight) {
    TEST_IGNORE_MESSAGE("UDP send did not enter in-flight state");
  }

  BacnetValue value;
  const auto status =
    session.readProperty(BacnetObjectType::AnalogValue, 200, BacnetPropertyId::PresentValue, value, 1);
  TEST_ASSERT_NOT_EQUAL_UINT8(
    static_cast<uint8_t>(BacnetDeviceSessionReadStatus::Busy),
    static_cast<uint8_t>(status));
}

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_bacnet_client_lifecycle);
  RUN_TEST(test_bacnet_logger_filters_global_and_output_levels);
  RUN_TEST(test_bacnet_logger_filters_tags_and_rate_limits);
  RUN_TEST(test_bacnet_logger_scoped_tags_and_no_output_mode);
  RUN_TEST(test_bacnet_logger_uses_bounded_output_storage);
  RUN_TEST(test_bacnet_logger_ignores_extra_scoped_tags);
  RUN_TEST(test_bacnet_logger_verbose_compile_gate);
  RUN_TEST(test_portable_protocol_builds_who_is_request);
  RUN_TEST(test_portable_protocol_parses_i_am_response);
  RUN_TEST(test_bacnet_client_rejects_non_i_am_response);
  RUN_TEST(test_portable_protocol_classifies_reject_and_abort);
  RUN_TEST(test_bacnet_client_builds_read_property_request);
  RUN_TEST(test_bacnet_client_builds_ai_present_value_request_with_type_zero);
  RUN_TEST(test_bacnet_property_ids_cover_generic_access_slice);
  RUN_TEST(test_bacnet_engineering_unit_symbol_mapping_is_small_and_safe);
  RUN_TEST(test_bacnet_display_value_and_status_helpers);
  RUN_TEST(test_bacnet_client_builds_mv_present_value_request);
  RUN_TEST(test_bacnet_client_builds_generic_property_request);
  RUN_TEST(test_bacnet_client_builds_object_list_array_request);
  RUN_TEST(test_bacnet_client_parses_read_property_ack);
  RUN_TEST(test_bacnet_client_parses_mv_present_value_ack);
  RUN_TEST(test_bacnet_client_parses_av_present_value_real_ack);
  RUN_TEST(test_bacnet_client_parses_ai_present_value_ack_with_type_zero);
  RUN_TEST(test_bacnet_client_parses_status_flags_bit_string_ack);
  RUN_TEST(test_bacnet_object_health_state_derivation);
  RUN_TEST(test_bacnet_client_parses_object_list_entry_ack);
  RUN_TEST(test_bacnet_client_parses_object_list_ack);
  RUN_TEST(test_bacnet_client_parses_read_property_error);
  RUN_TEST(test_bacnet_client_parses_read_property_error_codes);
  RUN_TEST(test_bacnet_client_parses_property_list_count_ack);
  RUN_TEST(test_bacnet_client_parses_property_list_entry_ack);
  RUN_TEST(test_bacnet_device_session_keeps_remote_device_metadata);
  RUN_TEST(test_bacnet_device_session_from_endpoint_keeps_metadata);
  RUN_TEST(test_bacnet_device_session_from_i_am_uses_default_port);
  RUN_TEST(test_bacnet_device_session_reports_send_failure);
  RUN_TEST(test_bacnet_device_session_caches_failed_read_status);
  RUN_TEST(test_bacnet_property_exposes_cached_read_state);
#if defined(UNIT_TEST)
  RUN_TEST(test_bacnet_property_cache_keeps_value_after_failed_refresh);
#endif
  RUN_TEST(test_bacnet_device_session_creates_remote_object);
  RUN_TEST(test_bacnet_device_session_creates_process_object_helpers);
  RUN_TEST(test_bacnet_remote_object_creates_property_request);
  RUN_TEST(test_bacnet_remote_object_read_reports_send_failure);
  RUN_TEST(test_bacnet_process_object_present_value_uses_generic_property_api);
  RUN_TEST(test_bacnet_process_object_metadata_uses_generic_property_api);
  RUN_TEST(test_bacnet_process_object_status_uses_generic_status_reads);
  RUN_TEST(test_bacnet_property_read_reports_send_failure);
  RUN_TEST(test_bacnet_remote_object_property_list_reports_send_failure);
  RUN_TEST(test_bacnet_remote_object_read_all_attempts_each_property);
  RUN_TEST(test_bacnet_remote_object_read_all_advertised_reports_property_list_failure);
  RUN_TEST(test_bacnet_object_scan_options_defaults);
  RUN_TEST(test_bacnet_object_scan_options_filter_object_types);
  RUN_TEST(test_bacnet_object_scan_options_accepts_analog_input_type_zero);
  RUN_TEST(test_bacnet_scanned_object_defaults_are_skipped);
  RUN_TEST(test_bacnet_device_session_scan_object_list_invalid_target);
  RUN_TEST(test_bacnet_object_list_scan_job_initial_state);
  RUN_TEST(test_bacnet_object_list_scan_job_reports_send_failure);
  RUN_TEST(test_bacnet_object_list_scan_job_busy_protects_blocking_read);
  RUN_TEST(test_bacnet_server_lifecycle);
  RUN_TEST(test_bacnet_subscribe_options_defaults);
  RUN_TEST(test_bacnet_property_subscription_is_move_only);
  RUN_TEST(test_bacnet_property_subscribe_exposes_identity_and_defaults);
  RUN_TEST(test_bacnet_subscription_request_refresh_and_fallback_zero_behavior);
  RUN_TEST(test_bacnet_subscription_stop_disables_polling);
  RUN_TEST(test_bacnet_subscription_notify_status_change_false_suppresses_callback);
  RUN_TEST(test_bacnet_subscription_immediate_first_read_works_with_fallback_zero);
  RUN_TEST(test_bacnet_subscription_move_while_in_flight_keeps_session_reference_safe);
  RUN_TEST(test_bacnet_subscription_stop_while_in_flight_allows_blocking_read);
  RUN_TEST(test_bacnet_subscription_destruction_while_in_flight_releases_session);
  UNITY_END();
}

void loop() {
}
