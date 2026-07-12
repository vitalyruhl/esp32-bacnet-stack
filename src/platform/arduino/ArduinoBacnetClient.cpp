// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "platform/arduino/ArduinoBacnetClient.h"

uint32_t ArduinoMonotonicClock::nowMs() const {
  return millis();
}

bool ArduinoUdpDatagramTransport::begin(uint16_t localPort) {
  return udp_.begin(localPort) == 1;
}

void ArduinoUdpDatagramTransport::end() {
  udp_.stop();
}

bool ArduinoUdpDatagramTransport::send(const BacnetIpEndpoint& destination,
                                       const uint8_t* data,
                                       size_t length) {
  const IPAddress address(destination.address[0], destination.address[1],
                          destination.address[2], destination.address[3]);
  if (udp_.beginPacket(address, destination.port) != 1) {
    return false;
  }
  return udp_.write(data, length) == length && udp_.endPacket() == 1;
}

size_t ArduinoUdpDatagramTransport::receive(uint8_t* data,
                                            size_t capacity,
                                            BacnetIpEndpoint& source) {
  const int packetSize = udp_.parsePacket();
  if (packetSize <= 0 || static_cast<size_t>(packetSize) > capacity) {
    return 0;
  }
  const int bytesRead = udp_.read(data, packetSize);
  if (bytesRead != packetSize) {
    return 0;
  }
  source = bacnetIpEndpointFromArduino(udp_.remoteIP(), udp_.remotePort());
  return static_cast<size_t>(bytesRead);
}

void ArduinoUdpDatagramTransport::idle() {
  yield();
}

void ArduinoSerialLogOutput::log(const BacnetLogRecord& record) {
  output_.print(BacnetLogger::levelPrefix(record.level));
  output_.print(' ');
  output_.print(record.tag != nullptr ? record.tag : "BACnet");
  output_.print(": ");
  output_.println(record.message != nullptr ? record.message : "");
}

BacnetIpEndpoint bacnetIpEndpointFromArduino(const IPAddress& address,
                                              uint16_t port) {
  return BacnetIpEndpoint(address[0], address[1], address[2], address[3], port);
}

IPAddress bacnetIpAddressFromEndpoint(const BacnetIpEndpoint& endpoint) {
  return IPAddress(endpoint.address[0], endpoint.address[1], endpoint.address[2],
                   endpoint.address[3]);
}
