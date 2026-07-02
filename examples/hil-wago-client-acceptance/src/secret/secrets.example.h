// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

// Copy this file to secrets.h and adjust local values there.
// The real secrets.h file is ignored by Git and must not be committed.

#define MY_WIFI_SSID "YOUR_WIFI_SSID"
#define MY_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Static IP settings. Use real local values only in the ignored secrets.h file.
#define MY_WIFI_IP "192.168.2.126"
#define MY_GATEWAY_IP "192.168.2.1"
#define MY_SUBNET_MASK "255.255.255.0"
#define MY_DNS_IP "192.168.2.1"
#define MY_USE_DHCP false

// Local WAGO BACnet/IP target.
// These defaults are examples for local acceptance validation only.
#define BACNET_TARGET_IP "192.168.2.101"
#define BACNET_TARGET_ADDRESS BACNET_TARGET_IP
#define BACNET_TARGET_PORT 47808
#define BACNET_TARGET_DEVICE_INSTANCE 1682101

// Expected value objects for scenario S01 (non-blocking object-list scan).
#define BACNET_EXPECT_AV_INSTANCE 200
#define BACNET_EXPECT_MV_INSTANCE 2000

// Optional acceptance expectations.
// Enable only when your local target really supports these checks.
#define HIL_EXPECT_MOVING_PRESENT_VALUE false
#define HIL_EXPECT_COV_SUPPORTED false

// Safety gates for BACnet write scenarios.
// Keep disabled unless an explicit local write validation is intended.
#define HIL_ENABLE_WRITE_TESTS false
#define HIL_ENABLE_PRIORITY_WRITE_TESTS false
