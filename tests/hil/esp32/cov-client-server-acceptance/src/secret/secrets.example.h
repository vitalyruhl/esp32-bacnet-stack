// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

// Copy this file to secrets.h and adjust local values there.
// The real secrets.h file is ignored by Git and must not be committed.

#define MY_WIFI_SSID "YOUR_WIFI_SSID"
#define MY_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Static IP settings. Use real local values only in the ignored secrets.h file.
#define MY_WIFI_IP "192.168.2.126"
// Optional wired override; otherwise the Ethernet env reuses MY_WIFI_IP.
// #define MY_ETHERNET_IP "192.168.2.127"
#define MY_GATEWAY_IP "192.168.2.1"
#define MY_SUBNET_MASK "255.255.255.0"
#define MY_DNS_IP "192.168.2.1"
#define MY_USE_DHCP false

// Local ESP32 BACnet/IP target.
// The COV environment overrides these defaults for the paired WiFi server.
#define BACNET_TARGET_IP "192.168.2.126"
#define BACNET_TARGET_ADDRESS BACNET_TARGET_IP
#define BACNET_TARGET_PORT 47808
#define BACNET_TARGET_DEVICE_INSTANCE 1682127

// Expected value objects for scenario S01 (non-blocking object-list scan).
// Set to 0 to stop requiring the corresponding object in S01.
#define BACNET_EXPECT_AV_INSTANCE 220
#define BACNET_EXPECT_MV_INSTANCE 2020

// Optional acceptance expectations.
// Enable only when your local target really supports these reads.
#define HIL_ENABLE_PROCESS_PRESENT_VALUE_READS true
#define HIL_ENABLE_PROCESS_STATUS_READS true
#define HIL_AI100_ANALOG_INPUT 100
#define HIL_REQUIRE_AI100 true
#define HIL_AI101_ANALOG_INPUT 101
#define HIL_REQUIRE_AI101 false
#define HIL_AO110_ANALOG_OUTPUT 110
#define HIL_REQUIRE_AO110 true
#define HIL_AO111_ANALOG_OUTPUT 111
#define HIL_REQUIRE_AO111 false
#define HIL_AV220_ANALOG_VALUE 220
#define HIL_REQUIRE_AV220 true
#define HIL_AV221_ANALOG_VALUE 221
#define HIL_REQUIRE_AV221 false
#define HIL_BI300_BINARY_INPUT 300
#define HIL_REQUIRE_BI300 true
#define HIL_BI301_BINARY_INPUT 301
#define HIL_REQUIRE_BI301 false
#define HIL_BO310_BINARY_OUTPUT 310
#define HIL_REQUIRE_BO310 true
#define HIL_BO311_BINARY_OUTPUT 311
#define HIL_REQUIRE_BO311 false
#define HIL_BV320_BINARY_VALUE 320
#define HIL_REQUIRE_BV320 true
#define HIL_BV321_BINARY_VALUE 321
#define HIL_REQUIRE_BV321 false
#define HIL_MI400_MULTISTATE_INPUT 400
#define HIL_REQUIRE_MI400 true
#define HIL_MI401_MULTISTATE_INPUT 401
#define HIL_REQUIRE_MI401 false
#define HIL_MO410_MULTISTATE_OUTPUT 410
#define HIL_REQUIRE_MO410 true
#define HIL_MO411_MULTISTATE_OUTPUT 411
#define HIL_REQUIRE_MO411 false
#define HIL_MV2020_MULTISTATE_VALUE 2020
#define HIL_REQUIRE_MV2020 true
#define HIL_MV2021_MULTISTATE_VALUE 2021
#define HIL_REQUIRE_MV2021 false
#define HIL_EXPECT_MOVING_PRESENT_VALUE false
#define HIL_EXPECT_COV_SUPPORTED false

// Safety gates for BACnet write scenarios.
// Keep disabled unless an explicit local write validation is intended.
#define HIL_ENABLE_WRITE_TESTS false
#define HIL_ENABLE_PRIORITY_WRITE_TESTS false

// Explicit Priority Write HIL targets. Keep the scenario disabled by default.
// A local target application may own its own values at priority 16; this
// scenario writes only temporary values at priority 8 and relinquishes them.
#define HIL_PRIORITY_WRITE_AV_INSTANCE 200
#define HIL_PRIORITY_WRITE_BV_INSTANCE 320
#define HIL_PRIORITY_WRITE_MSV_INSTANCE 2020
