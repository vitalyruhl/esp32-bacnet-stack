// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

// Copy this file to secrets.h and adjust local values there.
// The real secrets.h file is ignored by Git and must not be committed.

#define APP_NAME "BACnet Object List Scan Basic"

#define MY_WIFI_SSID "YOUR_WIFI_SSID"
#define MY_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Static IP settings. Use real local values only in the ignored secrets.h file.
#define MY_WIFI_IP "192.0.2.126"
#define MY_GATEWAY_IP "192.0.2.1"
#define MY_SUBNET_MASK "255.255.255.0"
#define MY_DNS_IP "192.0.2.1"
#define MY_USE_DHCP false

// BACnet validation target. Keep real local values in secrets.h.
#define BACNET_TARGET_DEVICE_INSTANCE 1234
#define BACNET_TARGET_ADDRESS_OCTETS 192, 0, 2, 101
#define BACNET_TARGET_PORT 47808
