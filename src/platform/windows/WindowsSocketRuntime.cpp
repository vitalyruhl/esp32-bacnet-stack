// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "platform/windows/WindowsSocketRuntime.h"

#include <winsock2.h>

#include <mutex>

namespace {
std::mutex runtimeMutex;
size_t runtimeReferences = 0;
} // namespace

WindowsSocketRuntime::~WindowsSocketRuntime() {
  end();
}

bool WindowsSocketRuntime::begin() {
  if (ready_) {
    return true;
  }

  std::lock_guard<std::mutex> lock(runtimeMutex);
  if (runtimeReferences == 0) {
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
      status_ = WindowsSocketRuntimeStatus::StartupFailed;
      return false;
    }
  }

  ++runtimeReferences;
  ready_ = true;
  status_ = WindowsSocketRuntimeStatus::Ready;
  return true;
}

void WindowsSocketRuntime::end() {
  if (!ready_) {
    return;
  }

  std::lock_guard<std::mutex> lock(runtimeMutex);
  if (runtimeReferences > 0 && --runtimeReferences == 0) {
    WSACleanup();
  }
  ready_ = false;
}

bool WindowsSocketRuntime::isReady() const {
  return ready_;
}

WindowsSocketRuntimeStatus WindowsSocketRuntime::status() const {
  return status_;
}
