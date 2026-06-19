// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include <BacnetClient.h>
#include <BacnetServer.h>
#include <unity.h>

void test_bacnet_client_lifecycle() {
  BacnetClient client;

  TEST_ASSERT_FALSE(client.isRunning());
  TEST_ASSERT_TRUE(client.begin());
  TEST_ASSERT_TRUE(client.isRunning());
  TEST_ASSERT_EQUAL_UINT16(BacnetClient::kDefaultPort, client.localPort());

  client.end();
  TEST_ASSERT_FALSE(client.isRunning());
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
  RUN_TEST(test_bacnet_server_lifecycle);
  UNITY_END();
}

void loop() {
}
