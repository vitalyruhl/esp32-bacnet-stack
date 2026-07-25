// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

// Copy this file to secrets.h and adjust local values there.
// The real secrets.h file is ignored by Git and must not be committed.
#define APP_NAME "ESP-to-ESP WiFi BACnet Client"

#define MY_WIFI_SSID "YOUR_WIFI_SSID"
#define MY_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define MY_WIFI_IP "192.168.2.126"
#define MY_GATEWAY_IP "192.168.2.1"
#define MY_SUBNET_MASK "255.255.255.0"
#define MY_DNS_IP "192.168.2.1"
#define MY_USE_DHCP false

#define SETTINGS_PASSWORD ""
#define OTA_PASSWORD ""

// Optional MAC-priority hint for ConfigManager access point selection.
// Use a real local value only in the ignored secrets.h file.
// #define WIFI_FILTER_MAC_PRIORITY "02:00:00:00:00:01"

#define BACNET_WHOIS_DESTINATION "192.168.2.255"
#define BACNET_TARGET_ADDRESS "192.168.2.127"
#define BACNET_TARGET_DEVICE_INSTANCE 1682127
