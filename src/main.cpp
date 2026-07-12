// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include <ArduinoBacnetServer.h>

#ifndef PIO_UNIT_TESTING
void setup() {
  BacnetServer server;

  (void)server;
}

void loop() {
}
#endif
