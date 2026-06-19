// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include <Arduino.h>
#include <EspBacnet.h>

BacnetServer bacnetServer;

void setup() {
  Serial.begin(115200);
  bacnetServer.begin(1234);
  Serial.println("[I] BACnet server demo started");
}

void loop() {
  delay(1000);
}
