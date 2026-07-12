// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "platform/windows/WindowsBacnetDatagramTransport.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <limits>
#include <thread>

namespace {
constexpr uintptr_t kInvalidSocket = (std::numeric_limits<uintptr_t>::max)();

SOCKET asSocket(uintptr_t socket) {
  return static_cast<SOCKET>(socket);
}

bool socketAddressFromEndpoint(const BacnetIpEndpoint& endpoint,
                               sockaddr_in& address,
                               bool allowPortZero) {
  if (!allowPortZero && !WindowsBacnetDatagramTransport::isValidRemoteEndpoint(endpoint)) {
    return false;
  }

  address = sockaddr_in{};
  address.sin_family = AF_INET;
  address.sin_port = htons(endpoint.port);
  address.sin_addr.S_un.S_un_b.s_b1 = endpoint.address[0];
  address.sin_addr.S_un.S_un_b.s_b2 = endpoint.address[1];
  address.sin_addr.S_un.S_un_b.s_b3 = endpoint.address[2];
  address.sin_addr.S_un.S_un_b.s_b4 = endpoint.address[3];
  return true;
}

BacnetIpEndpoint endpointFromSocketAddress(const sockaddr_in& address) {
  const auto& bytes = address.sin_addr.S_un.S_un_b;
  return BacnetIpEndpoint(bytes.s_b1, bytes.s_b2, bytes.s_b3, bytes.s_b4, ntohs(address.sin_port));
}
} // namespace

WindowsBacnetDatagramTransport::WindowsBacnetDatagramTransport(
  WindowsSocketRuntime& runtime)
    : runtime_(runtime), socket_(kInvalidSocket) {}

WindowsBacnetDatagramTransport::~WindowsBacnetDatagramTransport() {
  end();
}

bool WindowsBacnetDatagramTransport::begin(uint16_t localPort) {
  end();
  if (!runtime_.isReady()) {
    status_ = WindowsBacnetTransportStatus::RuntimeUnavailable;
    return false;
  }

  const SOCKET socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socket == INVALID_SOCKET) {
    status_ = WindowsBacnetTransportStatus::SocketCreateFailed;
    return false;
  }

  u_long nonBlocking = 1;
  BOOL broadcast = TRUE;
  if (ioctlsocket(socket, FIONBIO, &nonBlocking) != 0 ||
      setsockopt(socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcast), sizeof(broadcast)) != 0) {
    closesocket(socket);
    status_ = WindowsBacnetTransportStatus::SocketConfigureFailed;
    return false;
  }

  BacnetIpEndpoint bindEndpoint = bindAddress_;
  bindEndpoint.port = localPort;
  sockaddr_in bindAddress{};
  if (!socketAddressFromEndpoint(bindEndpoint, bindAddress, true) ||
      bind(socket, reinterpret_cast<const sockaddr*>(&bindAddress), sizeof(bindAddress)) == SOCKET_ERROR) {
    closesocket(socket);
    status_ = WindowsBacnetTransportStatus::BindFailed;
    return false;
  }

  sockaddr_in resolvedAddress{};
  int resolvedAddressSize = sizeof(resolvedAddress);
  if (getsockname(socket, reinterpret_cast<sockaddr*>(&resolvedAddress), &resolvedAddressSize) == SOCKET_ERROR) {
    closesocket(socket);
    status_ = WindowsBacnetTransportStatus::BindFailed;
    return false;
  }

  socket_ = static_cast<uintptr_t>(socket);
  localEndpoint_ = endpointFromSocketAddress(resolvedAddress);
  status_ = WindowsBacnetTransportStatus::None;
  return true;
}

bool WindowsBacnetDatagramTransport::setBindAddress(
  const BacnetIpEndpoint& endpoint) {
  if (isOpen()) {
    return false;
  }
  bindAddress_ = endpoint;
  bindAddress_.port = 0;
  return true;
}

void WindowsBacnetDatagramTransport::end() {
  if (socket_ != kInvalidSocket) {
    closesocket(asSocket(socket_));
  }
  socket_ = kInvalidSocket;
  localEndpoint_ = BacnetIpEndpoint{};
}

bool WindowsBacnetDatagramTransport::send(const BacnetIpEndpoint& destination,
                                          const uint8_t* data,
                                          size_t length) {
  if (!isOpen() || (data == nullptr && length != 0)) {
    status_ = WindowsBacnetTransportStatus::InvalidPayload;
    return false;
  }

  sockaddr_in destinationAddress{};
  if (!socketAddressFromEndpoint(destination, destinationAddress, false)) {
    status_ = WindowsBacnetTransportStatus::InvalidEndpoint;
    return false;
  }

  const int sent = sendto(asSocket(socket_), reinterpret_cast<const char*>(data), static_cast<int>(length), 0, reinterpret_cast<const sockaddr*>(&destinationAddress), sizeof(destinationAddress));
  if (sent != static_cast<int>(length)) {
    status_ = WindowsBacnetTransportStatus::SendFailed;
    return false;
  }
  status_ = WindowsBacnetTransportStatus::None;
  return true;
}

size_t WindowsBacnetDatagramTransport::receive(uint8_t* data,
                                               size_t capacity,
                                               BacnetIpEndpoint& source) {
  if (!isOpen() || data == nullptr || capacity == 0) {
    status_ = WindowsBacnetTransportStatus::InvalidPayload;
    return 0;
  }

  sockaddr_in sourceAddress{};
  int sourceAddressSize = sizeof(sourceAddress);
  const int received = recvfrom(asSocket(socket_), reinterpret_cast<char*>(data), static_cast<int>(capacity), 0, reinterpret_cast<sockaddr*>(&sourceAddress), &sourceAddressSize);
  if (received == SOCKET_ERROR) {
    if (WSAGetLastError() == WSAEWOULDBLOCK) {
      status_ = WindowsBacnetTransportStatus::None;
      return 0;
    }
    status_ = WindowsBacnetTransportStatus::ReceiveFailed;
    return 0;
  }

  source = endpointFromSocketAddress(sourceAddress);
  status_ = WindowsBacnetTransportStatus::None;
  return static_cast<size_t>(received);
}

void WindowsBacnetDatagramTransport::idle() {
  std::this_thread::yield();
}

bool WindowsBacnetDatagramTransport::isOpen() const {
  return socket_ != kInvalidSocket;
}

BacnetIpEndpoint WindowsBacnetDatagramTransport::localEndpoint() const {
  return localEndpoint_;
}

WindowsBacnetTransportStatus WindowsBacnetDatagramTransport::status() const {
  return status_;
}

bool WindowsBacnetDatagramTransport::isValidRemoteEndpoint(
  const BacnetIpEndpoint& endpoint) {
  return endpoint.port != 0;
}
