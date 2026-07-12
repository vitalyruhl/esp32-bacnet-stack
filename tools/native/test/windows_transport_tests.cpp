// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "platform/windows/WindowsBacnetDatagramTransport.h"
#include "platform/windows/WindowsSocketRuntime.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>

namespace {
bool expect(bool condition, const char* text) {
  if (!condition) {
    std::fprintf(stderr, "[E] %s\n", text);
  }
  return condition;
}
} // namespace

int main() {
  WindowsSocketRuntime firstRuntime;
  WindowsSocketRuntime secondRuntime;
  if (!expect(firstRuntime.begin(), "first Winsock runtime failed") ||
      !expect(secondRuntime.begin(), "second Winsock runtime failed")) {
    return 1;
  }

  WindowsBacnetDatagramTransport receiver(firstRuntime);
  WindowsBacnetDatagramTransport sender(secondRuntime);
  if (!expect(receiver.setBindAddress(BacnetIpEndpoint(127, 0, 0, 1, 47808)),
              "receiver bind address configuration failed") ||
      !expect(receiver.begin(0), "receiver bind failed") ||
      !expect(sender.begin(0), "sender bind failed")) {
    return 1;
  }

  const BacnetIpEndpoint receiverEndpoint = receiver.localEndpoint();
  if (!expect(receiverEndpoint.port != 0, "dynamic receiver port was not assigned") ||
      !expect(receiverEndpoint.address[0] == 127 && receiverEndpoint.address[1] == 0 &&
                receiverEndpoint.address[2] == 0 && receiverEndpoint.address[3] == 1,
              "receiver bind endpoint is not loopback")) {
    return 1;
  }

  if (!expect(WindowsBacnetDatagramTransport::isValidRemoteEndpoint(
                BacnetIpEndpoint(127, 0, 0, 1, 47808)),
              "loopback endpoint was rejected") ||
      !expect(WindowsBacnetDatagramTransport::isValidRemoteEndpoint(
                BacnetIpEndpoint(0, 0, 0, 0, 47808)),
              "any-address endpoint was rejected") ||
      !expect(WindowsBacnetDatagramTransport::isValidRemoteEndpoint(
                BacnetIpEndpoint(255, 255, 255, 255, 47808)),
              "broadcast endpoint was rejected") ||
      !expect(!WindowsBacnetDatagramTransport::isValidRemoteEndpoint(
                BacnetIpEndpoint(127, 0, 0, 1, 0)),
              "zero-port endpoint was accepted")) {
    return 1;
  }

  const BacnetIpEndpoint loopback(127, 0, 0, 1, receiverEndpoint.port);
  const uint8_t payload[] = {0x42, 0x43};
  if (!expect(sender.send(loopback, payload, sizeof(payload)), "loopback send failed") ||
      !expect(!sender.send(BacnetIpEndpoint(127, 0, 0, 1, 0), payload, sizeof(payload)),
              "port zero send was accepted") ||
      !expect(!sender.send(loopback, nullptr, 1),
              "invalid payload was accepted")) {
    return 1;
  }

  uint8_t received[sizeof(payload)] = {};
  BacnetIpEndpoint source;
  size_t receivedSize = 0;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (std::chrono::steady_clock::now() < deadline && receivedSize == 0) {
    receivedSize = receiver.receive(received, sizeof(received), source);
    if (receivedSize == 0) {
      std::this_thread::yield();
    }
  }

  if (!expect(receivedSize == sizeof(payload), "loopback receive size mismatch") ||
      !expect(received[0] == payload[0] && received[1] == payload[1],
              "loopback payload mismatch") ||
      !expect(source.address[0] == 127 && source.address[1] == 0 &&
                source.address[2] == 0 && source.address[3] == 1,
              "loopback source endpoint mismatch")) {
    return 1;
  }

  uint8_t noData[1] = {};
  BacnetIpEndpoint noDataSource;
  if (!expect(receiver.receive(noData, sizeof(noData), noDataSource) == 0,
              "empty poll returned data") ||
      !expect(receiver.status() == WindowsBacnetTransportStatus::None,
              "empty poll reported an error")) {
    return 1;
  }

  sender.end();
  sender.end();
  receiver.end();
  receiver.end();
  secondRuntime.end();
  secondRuntime.end();
  firstRuntime.end();
  firstRuntime.end();
  return 0;
}
