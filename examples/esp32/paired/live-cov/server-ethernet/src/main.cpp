// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

// Reuse the paired live-COV server wrapper with its Ethernet transport.
#define BACNET_DEMO_USE_ETHERNET 1
#if __has_include("secret/secrets.h")
#include "secret/secrets.h"
#define BACNET_DEMO_HAS_WIFI_SECRETS 1
#else
#define BACNET_DEMO_HAS_WIFI_SECRETS 0
#endif
#ifdef APP_NAME
#define ESP_TO_ESP_SERVER_APP_NAME APP_NAME
#else
#define ESP_TO_ESP_SERVER_APP_NAME "ESP-to-ESP Ethernet BACnet Server"
#endif
#define ESP_TO_ESP_SERVER_NVS_NAMESPACE "esp2esp_srv_eth"
#include "../../server-wifi/src/main.cpp"
