// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

// Copy this file to secrets.h and adjust local values there.
// The real secrets.h file is ignored by Git and must not be committed.

#define MY_WIFI_SSID "YOUR_WIFI_SSID"
#define MY_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// DHCP is enabled by default. The static values below are documentation-only.
#define MY_WIFI_IP "192.0.2.127"
#define MY_ETHERNET_IP "192.0.2.128"
#define MY_GATEWAY_IP "192.168.2.1"
#define MY_SUBNET_MASK "255.255.255.0"
#define MY_DNS_IP "192.168.2.1"
#define MY_USE_DHCP true
