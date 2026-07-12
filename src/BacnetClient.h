// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include "BacnetFeatureGates.h"
#include "BacnetLogger.h"
#include "portable/BacnetProtocol.h"
#include "portable/BacnetRuntime.h"

struct BacnetIAmDevice {
  BacnetIpEndpoint endpoint;
  uint32_t deviceInstance = 0;
  uint32_t maxApduLengthAccepted = 0;
  uint8_t segmentationSupported = 0;
  uint16_t vendorId = 0;
};

class BacnetClient {
public:
  static constexpr uint16_t kDefaultPort = 47808;
  static constexpr size_t kWhoIsRequestSize = 8;
  static constexpr size_t kMaxReadPropertyRequestSize = 25;
  static constexpr size_t kMaxWritePropertyRequestSize = BacnetProtocol::kMaxWritePropertyRequestSize;
  static constexpr size_t kMaxSubscribeCovRequestSize = 32;
  static constexpr uint32_t kNoArrayIndex = kBacnetNoArrayIndex;

  // Compatibility constructor. Bind an explicit transport before calling begin().
  BacnetClient();
  explicit BacnetClient(BacnetDatagramTransport& transport,
                        const BacnetMonotonicClock* clock = nullptr);

  bool begin(uint16_t localPort = kDefaultPort);
  void end();

  bool isRunning() const;
  uint16_t localPort() const;
  BacnetLogger& logger();
  const BacnetLogger& logger() const;
  bool setTransport(BacnetDatagramTransport& transport);
  void setClock(const BacnetMonotonicClock* clock);
  uint32_t nowMs() const;
  void idle();

  bool sendWhoIs(const BacnetIpEndpoint& destination);
  bool sendSubscribeCov(const BacnetIpEndpoint& destination,
                        uint32_t processId,
                        BacnetObjectId object,
                        uint32_t lifetimeSeconds,
                        uint8_t invokeId = 1);
  BacnetSubscribeCovResponseKind pollSubscribeCov(uint8_t expectedInvokeId);
  bool pollCovNotification(BacnetCovNotification& notification);
  bool pollIAm(BacnetIAmDevice& device);
  bool sendReadProperty(const BacnetIpEndpoint& destination,
                        const BacnetPropertyRequest& request,
                        uint8_t invokeId = 1);
  bool sendReadProperty(const BacnetIpEndpoint& destination,
                        BacnetObjectId object,
                        BacnetPropertyId property,
                        uint8_t invokeId = 1,
                        uint32_t arrayIndex = kNoArrayIndex);
  BacnetWritePropertyPollStatus sendWriteProperty(
    const BacnetIpEndpoint& destination,
    const BacnetPropertyRequest& request,
    const BacnetValue& value,
    uint8_t invokeId = 1);
  BacnetWritePropertyPollStatus sendWriteProperty(
    const BacnetIpEndpoint& destination, BacnetObjectId object, BacnetPropertyId property, const BacnetValue& value, const BacnetWritePropertyOptions& options, uint8_t invokeId = 1);
  BacnetWritePropertyPollStatus pollWriteProperty(uint8_t expectedInvokeId);
  bool pollReadProperty(BacnetValue& value, uint8_t expectedInvokeId, const BacnetPropertyRequest& expectedRequest);
  bool pollReadProperty(BacnetValue& value, uint8_t expectedInvokeId, BacnetPropertyId expectedProperty);
  void logReadPropertyTimeout(uint8_t invokeId,
                              const BacnetPropertyRequest& request);
  BacnetReadPropertyPollStatus pollReadPropertyStatus(
    BacnetValue& value, uint8_t expectedInvokeId, const BacnetPropertyRequest& expectedRequest);
  BacnetReadPropertyPollStatus pollReadPropertyStatus(
    BacnetValue& value, uint8_t expectedInvokeId, const BacnetPropertyRequest& expectedRequest, uint32_t* errorClass, uint32_t* errorCode);
  BacnetReadPropertyPollStatus pollReadPropertyStatus(
    BacnetValue& value, uint8_t expectedInvokeId, BacnetPropertyId expectedProperty);
  BacnetReadPropertyPollStatus pollReadPropertyStatus(
    BacnetValue& value, uint8_t expectedInvokeId, BacnetPropertyId expectedProperty, uint32_t* errorClass, uint32_t* errorCode);

  static size_t buildWhoIsRequest(uint8_t* buffer, size_t bufferSize);
  static bool parseIAmResponse(const uint8_t* buffer, size_t length, BacnetIAmDevice& device);
  static size_t buildReadPropertyRequest(uint8_t* buffer, size_t bufferSize, const BacnetPropertyRequest& request, uint8_t invokeId = 1);
  static size_t buildWritePropertyRequest(uint8_t* buffer, size_t bufferSize, const BacnetPropertyRequest& request, const BacnetValue& value, uint8_t invokeId = 1);
  static size_t buildWritePropertyRequest(uint8_t* buffer, size_t bufferSize, BacnetObjectId object, BacnetPropertyId property, const BacnetValue& value, const BacnetWritePropertyOptions& options, uint8_t invokeId = 1);
  static size_t buildReadPropertyRequest(uint8_t* buffer, size_t bufferSize, BacnetObjectId object, BacnetPropertyId property, uint8_t invokeId = 1, uint32_t arrayIndex = kNoArrayIndex);
  static bool parseReadPropertyAck(const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, const BacnetPropertyRequest& expectedRequest, BacnetValue& value);
  static bool parseReadPropertyAck(const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, BacnetPropertyId expectedProperty, BacnetValue& value);
  static bool parseReadPropertyError(const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, BacnetValue& value);
  static bool parseReadPropertyError(const uint8_t* buffer, size_t length, uint8_t expectedInvokeId, BacnetValue& value, uint32_t* errorClass, uint32_t* errorCode);

private:
  static constexpr size_t kMaxDiscoveryPacketSize = 512;

  BacnetDatagramTransport* transport_ = nullptr;
  BacnetLogger logger_;
  bool running_ = false;
  uint16_t localPort_ = kDefaultPort;
};
