// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include <Arduino.h>
#include <EspBacnet.h>

#ifndef PIO_UNIT_TESTING
void setup() {
  BacnetClient client;
  BacnetServer server;

  (void)client;
  (void)server;
}

void loop() {
}
#endif
