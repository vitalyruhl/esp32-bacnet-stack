// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include <BacnetClient.h>
#include <BacnetServer.h>
#include <unity.h>

void test_bacnet_client_lifecycle() {
  BacnetClient client;

  TEST_ASSERT_FALSE(client.isRunning());
  TEST_ASSERT_TRUE(client.begin(47809));
  TEST_ASSERT_TRUE(client.isRunning());
  TEST_ASSERT_EQUAL_UINT16(47809, client.localPort());

  client.end();
  TEST_ASSERT_FALSE(client.isRunning());
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
      0x00, 0x23, 0x29, 0x22, 0x05, 0xC4, 0x91, 0x00, 0x21, 0xDE,
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
      0x0C, 0x0C, 0x02, 0x00, 0x23, 0x29, 0x19, 0x4D,
  };

  TEST_ASSERT_EQUAL_UINT32(
      sizeof(expected),
      BacnetClient::buildReadPropertyRequest(
          request, sizeof(request), BacnetObjectId{8, 9001},
          BacnetPropertyId::ObjectName, 1));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, request, sizeof(expected));
}

void test_bacnet_client_parses_read_property_ack() {
  const uint8_t response[] = {
      0x81, 0x0A, 0x00, 0x19, 0x01, 0x00, 0x30, 0x01, 0x0C,
      0x0C, 0x02, 0x00, 0x23, 0x29, 0x19, 0x4D, 0x3E, 0x75,
      0x05, 0x00, 0x57, 0x41, 0x47, 0x4F, 0x3F,
  };
  BacnetValue value;

  TEST_ASSERT_TRUE(BacnetClient::parseReadPropertyAck(
      response, sizeof(response), 1, BacnetPropertyId::ObjectName, value));
  TEST_ASSERT_EQUAL_STRING("WAGO", value.text);
  TEST_ASSERT_EQUAL_UINT32(4, value.textLength);
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
  RUN_TEST(test_bacnet_client_builds_who_is_request);
  RUN_TEST(test_bacnet_client_parses_i_am_response);
  RUN_TEST(test_bacnet_client_rejects_non_i_am_response);
  RUN_TEST(test_bacnet_client_builds_read_property_request);
  RUN_TEST(test_bacnet_client_parses_read_property_ack);
  RUN_TEST(test_bacnet_server_lifecycle);
  UNITY_END();
}

void loop() {
}
