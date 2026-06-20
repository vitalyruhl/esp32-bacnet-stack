// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

// Copy this file to secrets.h and adjust values.
// The real secrets.h file is ignored by Git.

#define APP_NAME "BACnet Client Discovery Demo"

#define MY_WIFI_SSID "YOUR_WIFI_SSID"
#define MY_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Optional static IP settings. DHCP is enabled by default.
#define MY_WIFI_IP "192.168.2.126"
#define MY_GATEWAY_IP "192.168.2.250"
#define MY_SUBNET_MASK "255.255.255.0"
#define MY_DNS_IP "192.168.2.250"
#define MY_USE_DHCP true

#define SETTINGS_PASSWORD ""
#define OTA_PASSWORD ""

// Optional MAC-priority hint for ConfigManager access point selection.
// #define WIFI_FILTER_MAC_PRIORITY "60:B5:8D:4C:E1:D5"
