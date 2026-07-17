// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include <BacnetDeviceSession.h>

#include <cstddef>
#include <cstdint>
#include <memory>

class BacnetDemoPropertyBrowser {
public:
  static constexpr size_t kMaxProperties = 8;

  void reset();
  void load(BacnetDeviceSession& session,
            BacnetObjectId object,
            uint32_t timeoutMs);
  void applyReadAllResult(BacnetObjectId object,
                          const BacnetPropertyReadAllResult& summary,
                          const BacnetPropertyReadResult* rows,
                          size_t count);

  bool select(size_t index);
  bool hasSelection() const;
  size_t selectedIndex() const;
  bool subscribe(BacnetDeviceSession& session,
                 const BacnetSubscribeOptions& options);
  void stopSubscription();
  void poll(BacnetDeviceSession& session, uint32_t nowMs);

  bool loaded() const;
  BacnetObjectId objectId() const;
  const BacnetPropertyReadAllResult& result() const;
  bool usingFallbackProperties() const;
  size_t rowCount() const;
  const BacnetPropertyReadResult* row(size_t index) const;
  const BacnetPropertyReadResult* selectedRow() const;
  const BacnetPropertySubscription* subscription() const;
  const char* subscriptionModeText() const;

private:
  BacnetObjectId object_;
  BacnetPropertyReadAllResult summary_;
  BacnetPropertyReadResult rows_[kMaxProperties];
  size_t rowCount_ = 0;
  size_t selectedIndex_ = kMaxProperties;
  bool usingFallbackProperties_ = false;
  std::unique_ptr<BacnetPropertySubscription> subscription_;
};
