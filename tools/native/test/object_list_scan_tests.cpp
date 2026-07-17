// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetClient.h"
#include "BacnetDeviceSession.h"

#include <cstdio>
#include <cstring>

namespace {

int failures = 0;

bool expect(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "[E] %s\n", message);
    ++failures;
  }
  return condition;
}

class FakeClock final : public BacnetMonotonicClock {
public:
  uint32_t nowMs() const override { return now; }

  uint32_t now = 0;
};

class FakeTransport final : public BacnetDatagramTransport {
public:
  bool begin(uint16_t) override { return true; }
  void end() override {}

  bool send(const BacnetIpEndpoint&, const uint8_t* data, size_t length) override {
    ++sendCount;
    if (length <= sizeof(lastPacket)) {
      std::memcpy(lastPacket, data, length);
      lastPacketSize = length;
    }
    return true;
  }

  size_t receive(uint8_t* data, size_t capacity, BacnetIpEndpoint&) override {
    if (responseSize == 0 || responseSize > capacity) {
      return 0;
    }
    std::memcpy(data, response, responseSize);
    const size_t size = responseSize;
    responseSize = 0;
    return size;
  }

  void idle() override {}

  unsigned sendCount = 0;
  uint8_t lastPacket[64] = {};
  size_t lastPacketSize = 0;
  uint8_t response[64] = {};
  size_t responseSize = 0;
};

size_t buildReadError(uint8_t* buffer,
                      uint8_t invokeId,
                      uint8_t errorClass,
                      uint8_t errorCode) {
  const uint8_t response[] = {
    0x81, 0x0A, 0x00, 0x0D, 0x01, 0x00, 0x50, invokeId, 0x0C,
    0x91, errorClass, 0x91, errorCode,
  };
  std::memcpy(buffer, response, sizeof(response));
  return sizeof(response);
}

bool resultBufferIsEmpty(const BacnetScannedObject* results, size_t count) {
  for (size_t index = 0; index < count; ++index) {
    if (results[index].objectId.type != 0 || results[index].objectId.instance != 0) {
      return false;
    }
  }
  return true;
}

void testObjectListScanTimeoutDoesNotPopulateFallbackObjects() {
  FakeTransport transport;
  FakeClock clock;
  BacnetClient client(transport, &clock);
  BacnetDeviceSession session(
    client, 1682101, BacnetIpEndpoint(192, 0, 2, 101, BacnetClient::kDefaultPort));
  BacnetObjectListScanJob job;
  BacnetObjectScanOptions options;
  options.readTimeoutMs = 50;
  BacnetScannedObject results[3] = {};

  expect(client.begin(), "fake client must start");
  expect(session.beginObjectListScan(job, options, results, 3, clock.now),
         "object-list scan must start");
  expect(transport.sendCount == 1,
         "object-list scan must issue its count request before timing out");

  clock.now = options.readTimeoutMs;
  session.pollObjectListScan(job, clock.now);

  const BacnetObjectScanResult& summary = job.summary();
  expect(job.isFailed() && job.isTerminal() && !job.isActive(),
         "timed-out object-list scan must reach a failed terminal state");
  expect(!job.requestInFlight() && !session.isBusy(),
         "timed-out object-list scan must release the session");
  expect(summary.objectListCountStatus == BacnetDeviceSessionReadStatus::Timeout,
         "object-list count timeout must remain visible");
  expect(std::strcmp(bacnetReadStatusText(summary.objectListCountStatus), "timeout") == 0,
         "no success status may replace the object-list timeout");
  expect(summary.found == 0 && summary.stored == 0,
         "timed-out object-list scan must not report found or stored objects");
  expect(resultBufferIsEmpty(results, 3),
         "timed-out object-list scan must not insert configured fallback objects");

  BacnetCachedProperty cached;
  expect(session.cachedProperty(session.deviceObject(), BacnetPropertyId::ObjectList, cached, 0),
         "timed-out object-list count must remain available in the property cache");
  expect(cached.status == BacnetPropertyReadStatus::Timeout,
         "timed-out object-list count must not be classified as a fallback");
}

void testObjectListScanUnknownObjectDoesNotPopulateFallbackObjects() {
  FakeTransport transport;
  FakeClock clock;
  BacnetClient client(transport, &clock);
  BacnetDeviceSession session(
    client, 1682101, BacnetIpEndpoint(192, 0, 2, 101, BacnetClient::kDefaultPort));
  BacnetObjectListScanJob job;
  BacnetObjectScanOptions options;
  options.readTimeoutMs = 50;
  BacnetScannedObject results[3] = {};

  expect(client.begin(), "fake client must start");
  expect(session.beginObjectListScan(job, options, results, 3, clock.now),
         "object-list scan must start");
  expect(transport.lastPacketSize > 8,
         "object-list scan must retain the request invoke ID");
  transport.responseSize = buildReadError(
    transport.response,
    transport.lastPacket[8],
    1,
    31);

  session.pollObjectListScan(job, clock.now);

  const BacnetObjectScanResult& summary = job.summary();
  expect(job.isFailed() && job.isTerminal() && !job.isActive(),
         "unknown-object object-list scan must reach a failed terminal state");
  expect(!job.requestInFlight() && !session.isBusy(),
         "unknown-object object-list scan must release the session");
  expect(summary.objectListCountStatus == BacnetDeviceSessionReadStatus::Error,
         "unknown-object must remain a BACnet error instead of a scan success");
  expect(summary.found == 0 && summary.stored == 0,
         "unknown-object object-list scan must not report found or stored objects");
  expect(resultBufferIsEmpty(results, 3),
         "unknown-object object-list scan must not insert configured fallback objects");

  BacnetCachedProperty cached;
  expect(session.cachedProperty(session.deviceObject(), BacnetPropertyId::ObjectList, cached, 0),
         "unknown-object object-list count must remain available in the property cache");
  expect(cached.status == BacnetPropertyReadStatus::Error,
         "unknown-object must not be converted to unsupported-property");
  expect(cached.errorClass == 1 && cached.errorCode == 31,
         "unknown-object BACnet error class and code must be retained");
  expect(std::strcmp(bacnetReadStatusText(summary.objectListCountStatus, cached.value),
                     "unknown-object (object/31)") == 0,
         "unknown-object text must remain visible without an unsupported fallback");
}

}  // namespace

int main() {
  testObjectListScanTimeoutDoesNotPopulateFallbackObjects();
  testObjectListScanUnknownObjectDoesNotPopulateFallbackObjects();
  return failures == 0 ? 0 : 1;
}
