// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

enum class WindowsSocketRuntimeStatus {
  Ready,
  StartupFailed,
};

class WindowsSocketRuntime {
public:
  WindowsSocketRuntime() = default;
  ~WindowsSocketRuntime();

  WindowsSocketRuntime(const WindowsSocketRuntime&) = delete;
  WindowsSocketRuntime& operator=(const WindowsSocketRuntime&) = delete;

  bool begin();
  void end();
  bool isReady() const;
  WindowsSocketRuntimeStatus status() const;

private:
  bool ready_ = false;
  WindowsSocketRuntimeStatus status_ = WindowsSocketRuntimeStatus::StartupFailed;
};
