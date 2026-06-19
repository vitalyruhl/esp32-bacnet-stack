// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include <Arduino.h>
#include <EspBacnet.h>

BacnetClient bacnetClient;

void setup() {
  Serial.begin(115200);
  if (bacnetClient.begin()) {
    Serial.println("[I] BACnet client demo started");
    if (bacnetClient.sendWhoIs()) {
      Serial.println("[I] BACnet Who-Is discovery request sent");
    } else {
      Serial.println("[W] BACnet Who-Is discovery request was not sent");
    }
  } else {
    Serial.println("[E] BACnet client demo failed to start");
  }
}

void loop() {
  BacnetIAmDevice device;
  if (bacnetClient.pollIAm(device)) {
    Serial.print("[I] BACnet I-Am device=");
    Serial.print(device.deviceInstance);
    Serial.print(" maxApdu=");
    Serial.print(device.maxApduLengthAccepted);
    Serial.print(" vendor=");
    Serial.println(device.vendorId);
  }

  delay(1000);
}
