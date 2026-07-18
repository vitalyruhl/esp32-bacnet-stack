// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetServer.h"
#include "portable/BacnetDisplayText.h"

#include <cstring>

// ASHRAE-reserved vendor ID for tests and examples only; never a product ID.
constexpr uint16_t kTestVendorId = 555;

class SmokeTransport final : public BacnetDatagramTransport {
public:
  bool begin(uint16_t) override {
    return true;
  }
  void end() override {}
  bool send(const BacnetIpEndpoint& destination, const uint8_t* data, size_t length) override {
    if (data == nullptr || length > sizeof(lastSent)) {
      return false;
    }
    lastDestination = destination;
    lastSentLength = length;
    std::memcpy(lastSent, data, length);
    ++sendCalls;
    return true;
  }
  size_t receive(uint8_t* buffer, size_t capacity, BacnetIpEndpoint& source) override {
    if (incomingLength == 0 || incomingLength > capacity) {
      return 0;
    }
    std::memcpy(buffer, incoming, incomingLength);
    source = incomingSource;
    const size_t result = incomingLength;
    incomingLength = 0;
    return result;
  }
  void queue(const uint8_t* data, size_t length, const BacnetIpEndpoint& source) {
    if (data == nullptr || length > sizeof(incoming)) {
      incomingLength = 0;
      return;
    }
    std::memcpy(incoming, data, length);
    incomingLength = length;
    incomingSource = source;
  }
  void idle() override {}

  uint8_t incoming[BacnetServer::kMaxDatagramSize] = {};
  size_t incomingLength = 0;
  BacnetIpEndpoint incomingSource;
  uint8_t lastSent[BacnetServer::kMaxDatagramSize] = {};
  size_t lastSentLength = 0;
  BacnetIpEndpoint lastDestination;
  uint32_t sendCalls = 0;
};

int main() {
  uint8_t frame[BacnetProtocol::kWhoIsRequestSize] = {};
  const BacnetValue value{};
  SmokeTransport transport;
  BacnetServer server(transport);
  BacnetServerDevice device;
  device.deviceInstance = 1234;
  device.vendorId = kTestVendorId;
  device.maxApduLengthAccepted = 1024;
  const BacnetIpEndpoint source(192, 0, 2, 44, 47809);

  const bool baseline =
    BacnetProtocol::buildWhoIsRequest(frame, sizeof(frame)) == sizeof(frame) &&
    bacnetObjectTypePrefix(BacnetObjectType::AnalogValue)[0] == 'A' &&
    !bacnetValueInRange(value, 0.0f, 1.0f) && server.begin(device);
  if (!baseline) {
    return 1;
  }

  const uint8_t truncatedConfirmed[] = {
    0x81,
    0x0A,
    0x00,
    0x09,
    0x01,
    0x04,
    0x00,
    0x05,
    0x42,
  };
  transport.queue(truncatedConfirmed, sizeof(truncatedConfirmed), source);
  const bool truncation = server.poll() == BacnetServerPollResult::Malformed &&
                          transport.sendCalls == 0;

  transport.queue(frame, sizeof(frame), source);
  BacnetIAmDeviceInfo iam;
  const bool whoIs =
    server.poll() == BacnetServerPollResult::IAmSent &&
    transport.sendCalls == 1 && transport.lastDestination.port == source.port &&
    BacnetProtocol::parseIAmResponse(transport.lastSent, transport.lastSentLength, iam) &&
    iam.deviceInstance == device.deviceInstance && iam.vendorId == device.vendorId;

  const uint8_t includedRange[] = {
    0x81,
    0x0B,
    0x00,
    0x0E,
    0x01,
    0x00,
    0x10,
    0x08,
    0x0A,
    0x04,
    0xB0,
    0x1A,
    0x04,
    0xD2,
  };
  transport.queue(includedRange, sizeof(includedRange), source);
  const bool included = server.poll() == BacnetServerPollResult::IAmSent &&
                        transport.sendCalls == 2;

  const uint8_t excludedRange[] = {
    0x81,
    0x0B,
    0x00,
    0x0E,
    0x01,
    0x00,
    0x10,
    0x08,
    0x0A,
    0x04,
    0xD3,
    0x1A,
    0x04,
    0xD4,
  };
  transport.queue(excludedRange, sizeof(excludedRange), source);
  const bool excluded = server.poll() == BacnetServerPollResult::Ignored &&
                        transport.sendCalls == 2;

  const uint8_t incompleteRange[] = {
    0x81,
    0x0B,
    0x00,
    0x0A,
    0x01,
    0x00,
    0x10,
    0x08,
    0x09,
    0x2A,
  };
  transport.queue(incompleteRange, sizeof(incompleteRange), source);
  const bool incomplete = server.poll() == BacnetServerPollResult::Malformed &&
                          transport.sendCalls == 2;

  const uint8_t invalidRange[] = {
    0x81,
    0x0B,
    0x00,
    0x10,
    0x01,
    0x00,
    0x10,
    0x08,
    0x0B,
    0x40,
    0x00,
    0x00,
    0x1B,
    0x40,
    0x00,
    0x01,
  };
  transport.queue(invalidRange, sizeof(invalidRange), source);
  const bool invalid = server.poll() == BacnetServerPollResult::Malformed &&
                       transport.sendCalls == 2;

  const uint8_t confirmedReadProperty[] = {
    0x81,
    0x0A,
    0x00,
    0x0A,
    0x01,
    0x04,
    0x00,
    0x05,
    0x42,
    0x0C,
  };
  transport.queue(confirmedReadProperty, sizeof(confirmedReadProperty), source);
  const bool reject =
    server.poll() == BacnetServerPollResult::RejectSent &&
    transport.sendCalls == 3 && transport.lastSentLength == BacnetProtocol::kRejectResponseSize &&
    transport.lastSent[6] == 0x60 && transport.lastSent[7] == 0x42 &&
    transport.lastSent[8] == BacnetServer::kRejectReasonUnrecognizedService;

  return whoIs && included && excluded && truncation && incomplete && invalid && reject ? 0 : 1;
}
