// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <cstdint>

#include "portable/BacnetProtocol.h"
#include "portable/BacnetRuntime.h"

struct BacnetServerDevice {
  // Required BACnet Device identity and profile values.
  uint32_t deviceInstance = 0;
  // Supplied by the final device provider; no production ID is assigned here.
  uint16_t vendorId = 0;
  const char* objectName = nullptr;
  const char* vendorName = nullptr;
  const char* modelName = nullptr;
  const char* firmwareRevision = nullptr;
  // Optional. A null or empty value exposes firmwareRevision instead.
  const char* applicationSoftwareVersion = nullptr;
  uint32_t maxApduLengthAccepted = 1476;
  uint32_t apduTimeout = 3000;
  uint32_t numberOfApduRetries = 3;
  uint32_t databaseRevision = 0;
  uint8_t protocolVersion = 1;
  uint8_t protocolRevision = 14;
  uint8_t segmentationSupported = 3;
};

using BacnetServerAnalogValueProvider = float (*)(void* context);

// Caller-owned Analog Value configuration. The server borrows this array and
// does not allocate an object database. A provider is polled only while
// encoding Present_Value; a null provider exposes the stored presentValue.
struct BacnetServerAnalogValue {
  uint32_t instance = 0;
  const char* objectName = nullptr;
  float presentValue = 0.0F;
  uint32_t units = 0;
  bool outOfService = false;
  BacnetServerAnalogValueProvider presentValueProvider = nullptr;
  void* presentValueContext = nullptr;
};

enum class BacnetServerPollResult : uint8_t {
  NotRunning,
  NoDatagram,
  Ignored,
  Malformed,
  IAmSent,
  RejectSent,
  ReadPropertyAckSent,
  ReadPropertyErrorSent,
  SendFailed,
};

class BacnetServer {
public:
  static constexpr uint16_t kDefaultPort = 47808;
  static constexpr uint8_t kRejectReasonUnrecognizedService = 9;
  static constexpr size_t kMaxDatagramSize = 1476;

  BacnetServer() = default;
  explicit BacnetServer(BacnetDatagramTransport& transport);
  BacnetServer(const BacnetServer&) = delete;
  BacnetServer& operator=(const BacnetServer&) = delete;
  BacnetServer(BacnetServer&&) = delete;
  BacnetServer& operator=(BacnetServer&&) = delete;

  // The server borrows its transport; it must outlive the server and must not
  // be shared between concurrently running server instances.
  // This stateless MVP does not require a clock or logger yet.
  bool setTransport(BacnetDatagramTransport& transport);
  // Binds the borrowed transport to localPort. Omitting it uses BACnet/IP UDP
  // port 47808; this is the sole local-port configuration for the server.
  bool begin(const BacnetServerDevice& configuredDevice,
             uint16_t localPort = kDefaultPort);
  bool begin(uint32_t deviceInstanceValue, uint16_t localPort = kDefaultPort);
  void end();

  // The caller keeps the array and all referenced strings alive while the
  // server is running. Passing nullptr with zero entries unregisters all AVs.
  bool setAnalogValues(BacnetServerAnalogValue* analogValues,
                       size_t count);
  size_t analogValueCount() const;

  bool isRunning() const;
  const BacnetServerDevice& device() const;
  uint32_t deviceInstance() const;
  uint16_t port() const;
  BacnetServerPollResult poll();

private:
  bool sendIAm(const BacnetIpEndpoint& destination);
  bool sendReject(const BacnetIpEndpoint& destination, uint8_t invokeId);
  BacnetServerPollResult handleReadProperty(
    const BacnetIpEndpoint& source,
    const BacnetReadPropertyRequestHeader& request);
  bool readDeviceProperty(BacnetPropertyId property, BacnetValue& value) const;
  static bool readAnalogValueProperty(const BacnetServerAnalogValue& analogValue,
                                      BacnetPropertyId property,
                                      BacnetValue& value);
  const BacnetServerAnalogValue* findAnalogValue(uint32_t instance) const;
  static bool objectListEntry(const void* context,
                              size_t index,
                              BacnetObjectId& object);

  BacnetDatagramTransport* transport_ = nullptr; // Non-owning.
  bool running_ = false;
  BacnetServerDevice device_;
  uint16_t port_ = kDefaultPort;
  BacnetServerAnalogValue* analogValues_ = nullptr; // Caller-owned.
  size_t analogValueCount_ = 0;
};
