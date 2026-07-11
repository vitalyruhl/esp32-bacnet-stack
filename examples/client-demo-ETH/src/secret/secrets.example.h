// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

// Copy this file to secrets.h and adjust local values there.
// The real secrets.h file is ignored by Git and must not be committed.

#define APP_NAME "BACnet Client Demo ETH"

#define MY_ETHERNET_IP "192.168.2.127"
#define MY_GATEWAY_IP "192.168.2.1"
#define MY_SUBNET_MASK "255.255.255.0"
#define MY_DNS_IP "192.168.2.1"
#define MY_USE_DHCP false

#define SETTINGS_PASSWORD ""
#define OTA_PASSWORD ""

#define BACNET_WHOIS_DESTINATION "192.168.2.255"
#define BACNET_TARGET_ADDRESS "192.168.2.101"
// #define BACNET_TARGET_DEVICE_INSTANCE 1682101
// #define BACNET_TARGET_PORT 47808

#define BACNET_FALLBACK_ANALOG_OBJECT_INSTANCE 220
#define BACNET_FALLBACK_BINARY_OBJECT_INSTANCE 320
#define BACNET_FALLBACK_MULTISTATE_OBJECT_INSTANCE 2020
