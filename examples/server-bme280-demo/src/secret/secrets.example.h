// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

// Copy to secrets.h for an optional first-start WiFi bootstrap. The real file
// is ignored by Git; BACnet and sensor settings belong in ConfigurationManager.
#define APP_NAME "BACnet BME280 Server"
#define MY_WIFI_SSID "YOUR_WIFI_SSID"
#define MY_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define MY_WIFI_IP "192.0.2.126"
#define MY_GATEWAY_IP "192.0.2.1"
#define MY_SUBNET_MASK "255.255.255.0"
#define MY_DNS_IP "192.0.2.1"
#define MY_USE_DHCP true
#define SETTINGS_PASSWORD ""

// Optional local AP-selection hint. Keep real values only in secrets.h.
// #define WIFI_FILTER_MAC_PRIORITY "02:00:00:00:00:01"
