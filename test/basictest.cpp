// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include <BacnetClient.h>
#include <BacnetServer.h>
#include <unity.h>

#include <cstdio>
#include <cstring>

class CapturingBacnetLogOutput : public BacnetLogOutput {
 public:
  void log(const BacnetLogRecord& record) override {
    ++count;
    lastLevel = record.level;
    lastTimestampMs = record.timestampMs;
    snprintf(lastTag, sizeof(lastTag), "%s",
             record.tag != nullptr ? record.tag : "");
    snprintf(lastMessage, sizeof(lastMessage), "%s",
             record.message != nullptr ? record.message : "");
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

  logger.emit(BacnetLogRecord{BacnetLogLevel::Info, "BACnet/ReadProperty",
                              "filtered", 100, "BACnet", nullptr});
  TEST_ASSERT_EQUAL_UINT32(0, output.count);

  logger.emit(BacnetLogRecord{BacnetLogLevel::Info, "BACnet/Discovery",
                              "first", 100, "BACnet", nullptr});
  logger.emit(BacnetLogRecord{BacnetLogLevel::Info, "BACnet/Discovery",
                              "rate limited", 150, "BACnet", nullptr});
  logger.emit(BacnetLogRecord{BacnetLogLevel::Info, "BACnet/Discovery",
                              "second", 220, "BACnet", nullptr});

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

void test_bacnet_client_builds_who_is_request() {
  uint8_t request[BacnetClient::kWhoIsRequestSize] = {};
  const uint8_t expected[] = {0x81, 0x0B, 0x00, 0x08,
                              0x01, 0x00, 0x10, 0x08};

  TEST_ASSERT_EQUAL_UINT32(sizeof(expected),
                           BacnetClient::buildWhoIsRequest(request,
                                                           sizeof(request)));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, request, sizeof(expected));
  TEST_ASSERT_EQUAL_UINT32(0, BacnetClient::buildWhoIsRequest(nullptr, 0));
}

void test_bacnet_client_parses_i_am_response() {
  const uint8_t response[] = {
      0x81, 0x0A, 0x00, 0x14, 0x01, 0x00, 0x10, 0x00, 0xC4, 0x02,
      0x00, 0x04, 0xD2, 0x22, 0x05, 0xC4, 0x91, 0x00, 0x21, 0xDE,
  };
  BacnetIAmDevice device;

  TEST_ASSERT_TRUE(BacnetClient::parseIAmResponse(response, sizeof(response),
                                                  device));
  TEST_ASSERT_EQUAL_UINT32(1234, device.deviceInstance);
  TEST_ASSERT_EQUAL_UINT32(1476, device.maxApduLengthAccepted);
  TEST_ASSERT_EQUAL_UINT8(0, device.segmentationSupported);
  TEST_ASSERT_EQUAL_UINT16(222, device.vendorId);
}

void test_bacnet_client_rejects_non_i_am_response() {
  const uint8_t response[] = {0x81, 0x0B, 0x00, 0x08,
                              0x01, 0x00, 0x10, 0x08};
  BacnetIAmDevice device;

  TEST_ASSERT_FALSE(BacnetClient::parseIAmResponse(response, sizeof(response),
                                                   device));
}

void test_bacnet_client_builds_read_property_request() {
  uint8_t request[BacnetClient::kMaxReadPropertyRequestSize] = {};
  const uint8_t expected[] = {
      0x81, 0x0A, 0x00, 0x11, 0x01, 0x04, 0x00, 0x05, 0x01,
      0x0C, 0x0C, 0x02, 0x00, 0x04, 0xD2, 0x19, 0x4D,
  };

  TEST_ASSERT_EQUAL_UINT32(
      sizeof(expected),
      BacnetClient::buildReadPropertyRequest(
          request, sizeof(request), BacnetObjectId{8, 1234},
          BacnetPropertyId::ObjectName, 1));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, request, sizeof(expected));
}

void test_bacnet_property_ids_cover_generic_access_slice() {
  TEST_ASSERT_EQUAL_UINT32(28,
                           static_cast<uint32_t>(BacnetPropertyId::Description));
  TEST_ASSERT_EQUAL_UINT32(
      74, static_cast<uint32_t>(BacnetPropertyId::NumberOfStates));
  TEST_ASSERT_EQUAL_UINT32(76,
                           static_cast<uint32_t>(BacnetPropertyId::ObjectList));
  TEST_ASSERT_EQUAL_UINT32(77,
                           static_cast<uint32_t>(BacnetPropertyId::ObjectName));
  TEST_ASSERT_EQUAL_UINT32(85,
                           static_cast<uint32_t>(BacnetPropertyId::PresentValue));
  TEST_ASSERT_EQUAL_UINT32(87,
                           static_cast<uint32_t>(BacnetPropertyId::PriorityArray));
  TEST_ASSERT_EQUAL_UINT32(
      104, static_cast<uint32_t>(BacnetPropertyId::RelinquishDefault));
  TEST_ASSERT_EQUAL_UINT32(110,
                           static_cast<uint32_t>(BacnetPropertyId::StateText));
  TEST_ASSERT_EQUAL_UINT32(371,
                           static_cast<uint32_t>(BacnetPropertyId::PropertyList));
}

void test_bacnet_client_builds_mv_present_value_request() {
  uint8_t request[BacnetClient::kMaxReadPropertyRequestSize] = {};
  const uint8_t expected[] = {
      0x81, 0x0A, 0x00, 0x11, 0x01, 0x04, 0x00, 0x05, 0x02,
      0x0C, 0x0C, 0x04, 0xC0, 0x00, 0x03, 0x19, 0x55,
  };

  TEST_ASSERT_EQUAL_UINT32(
      sizeof(expected),
      BacnetClient::buildReadPropertyRequest(
          request, sizeof(request), BacnetObjectId{19, 3},
          BacnetPropertyId::PresentValue, 2));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, request, sizeof(expected));
}

void test_bacnet_client_builds_generic_property_request() {
  uint8_t request[BacnetClient::kMaxReadPropertyRequestSize] = {};
  const uint8_t expected[] = {
      0x81, 0x0A, 0x00, 0x13, 0x01, 0x04, 0x00, 0x05, 0x06, 0x0C,
      0x0C, 0x04, 0xC0, 0x07, 0xD0, 0x19, 0x6E, 0x29, 0x01,
  };
  const BacnetPropertyRequest propertyRequest{
      BacnetObjectId{static_cast<uint16_t>(BacnetObjectType::MultiStateValue),
                     2000},
      BacnetPropertyId::StateText, 1};

  TEST_ASSERT_EQUAL_UINT32(
      sizeof(expected),
      BacnetClient::buildReadPropertyRequest(
          request, sizeof(request), propertyRequest, 6));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, request, sizeof(expected));
}

void test_bacnet_client_builds_object_list_array_request() {
  uint8_t request[BacnetClient::kMaxReadPropertyRequestSize] = {};
  const uint8_t expected[] = {
      0x81, 0x0A, 0x00, 0x13, 0x01, 0x04, 0x00, 0x05, 0x03, 0x0C,
      0x0C, 0x02, 0x00, 0x04, 0xD2, 0x19, 0x4C, 0x29, 0x02,
  };

  TEST_ASSERT_EQUAL_UINT32(
      sizeof(expected),
      BacnetClient::buildReadPropertyRequest(
          request, sizeof(request), BacnetObjectId{8, 1234},
          BacnetPropertyId::ObjectList, 3, 2));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, request, sizeof(expected));
}

void test_bacnet_client_parses_read_property_ack() {
  const uint8_t response[] = {
      0x81, 0x0A, 0x00, 0x19, 0x01, 0x00, 0x30, 0x01, 0x0C,
      0x0C, 0x02, 0x00, 0x04, 0xD2, 0x19, 0x4D, 0x3E, 0x75,
      0x05, 0x00, 0x57, 0x41, 0x47, 0x4F, 0x3F,
  };
  BacnetValue value;

  TEST_ASSERT_TRUE(BacnetClient::parseReadPropertyAck(
      response, sizeof(response), 1, BacnetPropertyId::ObjectName, value));
  TEST_ASSERT_EQUAL_STRING("WAGO", value.text);
  TEST_ASSERT_EQUAL_UINT32(4, value.textLength);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetValueType::CharacterString),
                          static_cast<uint8_t>(value.type));
}

void test_bacnet_client_parses_mv_present_value_ack() {
  const uint8_t response[] = {
      0x81, 0x0A, 0x00, 0x14, 0x01, 0x00, 0x30, 0x02, 0x0C, 0x0C,
      0x04, 0xC0, 0x00, 0x03, 0x19, 0x55, 0x3E, 0x21, 0x03, 0x3F,
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
      0x81, 0x0A, 0x00, 0x17, 0x01, 0x00, 0x30, 0x07, 0x0C, 0x0C,
      0x00, 0x80, 0x00, 0xC8, 0x19, 0x55, 0x3E, 0x44, 0x41, 0x48,
      0x00, 0x00, 0x3F,
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

void test_bacnet_client_parses_object_list_entry_ack() {
  const uint8_t response[] = {
      0x81, 0x0A, 0x00, 0x19, 0x01, 0x00, 0x30, 0x03, 0x0C,
      0x0C, 0x02, 0x00, 0x04, 0xD2, 0x19, 0x4C, 0x29, 0x02,
      0x3E, 0xC4, 0x04, 0xC0, 0x00, 0x0B, 0x3F,
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
      0x81, 0x0A, 0x00, 0x21, 0x01, 0x00, 0x30, 0x05, 0x0C,
      0x0C, 0x02, 0x00, 0x04, 0xD2, 0x19, 0x4C, 0x3E,
      0xC4, 0x02, 0x00, 0x04, 0xD2, 0xC4, 0x04, 0xC0, 0x00,
      0x0B, 0xC4, 0x04, 0xC0, 0x00, 0x0C, 0x3F,
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
      0x81, 0x0A, 0x00, 0x0D, 0x01, 0x00, 0x50,
      0x04, 0x0C, 0x91, 0x02, 0x91, 0x20,
  };
  BacnetValue value;

  TEST_ASSERT_TRUE(BacnetClient::parseReadPropertyError(response,
                                                        sizeof(response), 4,
                                                        value));
  TEST_ASSERT_EQUAL_STRING("error class 2 code 32", value.text);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(BacnetValueType::Error),
                          static_cast<uint8_t>(value.type));
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

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_bacnet_client_lifecycle);
  RUN_TEST(test_bacnet_logger_filters_global_and_output_levels);
  RUN_TEST(test_bacnet_logger_filters_tags_and_rate_limits);
  RUN_TEST(test_bacnet_logger_scoped_tags_and_no_output_mode);
  RUN_TEST(test_bacnet_logger_uses_bounded_output_storage);
  RUN_TEST(test_bacnet_logger_ignores_extra_scoped_tags);
  RUN_TEST(test_bacnet_logger_verbose_compile_gate);
  RUN_TEST(test_bacnet_client_builds_who_is_request);
  RUN_TEST(test_bacnet_client_parses_i_am_response);
  RUN_TEST(test_bacnet_client_rejects_non_i_am_response);
  RUN_TEST(test_bacnet_client_builds_read_property_request);
  RUN_TEST(test_bacnet_property_ids_cover_generic_access_slice);
  RUN_TEST(test_bacnet_client_builds_mv_present_value_request);
  RUN_TEST(test_bacnet_client_builds_generic_property_request);
  RUN_TEST(test_bacnet_client_builds_object_list_array_request);
  RUN_TEST(test_bacnet_client_parses_read_property_ack);
  RUN_TEST(test_bacnet_client_parses_mv_present_value_ack);
  RUN_TEST(test_bacnet_client_parses_av_present_value_real_ack);
  RUN_TEST(test_bacnet_client_parses_object_list_entry_ack);
  RUN_TEST(test_bacnet_client_parses_object_list_ack);
  RUN_TEST(test_bacnet_client_parses_read_property_error);
  RUN_TEST(test_bacnet_server_lifecycle);
  UNITY_END();
}

void loop() {
}
