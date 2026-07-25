// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

// Reuse the paired live-COV client wrapper with the WiFi transport.
#include <WiFi.h>

#define ESP_TO_ESP_CLIENT_APP_NAME "ESP-to-ESP WiFi BACnet Client"
#define ESP_TO_ESP_CLIENT_NVS_NAMESPACE "esp2esp_cli_wifi"
#define ESP_TO_ESP_CLIENT_BASE_INCLUDE "../../client-wifi/src/generated_client_base.inc"
#include "../../client-ethernet/src/main.cpp"
