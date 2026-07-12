// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <cstdint>

#include "platform/windows/WindowsSocketRuntime.h"
#include "portable/BacnetRuntime.h"

enum class WindowsBacnetTransportStatus {
  None,
  RuntimeUnavailable,
  SocketCreateFailed,
  SocketConfigureFailed,
  BindFailed,
  SendFailed,
  ReceiveFailed,
  InvalidEndpoint,
  InvalidPayload,
};

class WindowsBacnetDatagramTransport final : public BacnetDatagramTransport {
public:
  explicit WindowsBacnetDatagramTransport(WindowsSocketRuntime& runtime);
  ~WindowsBacnetDatagramTransport() override;

  bool begin(uint16_t localPort) override;
  bool setBindAddress(const BacnetIpEndpoint& endpoint);
  void end() override;
  bool send(const BacnetIpEndpoint& destination,
            const uint8_t* data,
            size_t length) override;
  size_t receive(uint8_t* data,
                 size_t capacity,
                 BacnetIpEndpoint& source) override;
  void idle() override;

  bool isOpen() const;
  BacnetIpEndpoint localEndpoint() const;
  WindowsBacnetTransportStatus status() const;

  static bool isValidRemoteEndpoint(const BacnetIpEndpoint& endpoint);

private:
  WindowsSocketRuntime& runtime_;
  uintptr_t socket_ = 0;
  BacnetIpEndpoint bindAddress_;
  BacnetIpEndpoint localEndpoint_;
  WindowsBacnetTransportStatus status_ = WindowsBacnetTransportStatus::None;
};
