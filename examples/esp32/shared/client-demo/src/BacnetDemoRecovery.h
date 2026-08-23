// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#include "core/protocol/BacnetTypes.h"

#include <cstddef>
#include <cstdint>

enum class BacnetDemoRecoveryReason : uint8_t {
  None,
  InitialDiscoveryTimeout,
  EmptyObjectList,
  ScanFailed,
  PeerSubscriptionsLost,
  LocalIpChanged,
};

inline const char* bacnetDemoRecoveryReasonText(BacnetDemoRecoveryReason reason) {
  switch (reason) {
    case BacnetDemoRecoveryReason::None:
      return "none";
    case BacnetDemoRecoveryReason::InitialDiscoveryTimeout:
      return "target I-Am timeout";
    case BacnetDemoRecoveryReason::EmptyObjectList:
      return "object-list contains no usable values";
    case BacnetDemoRecoveryReason::ScanFailed:
      return "object-list scan failed";
    case BacnetDemoRecoveryReason::PeerSubscriptionsLost:
      return "all COV subscriptions lost";
    case BacnetDemoRecoveryReason::LocalIpChanged:
      return "local IP changed";
  }
  return "unknown";
}

class BacnetDemoRecovery {
public:
  static constexpr uint32_t kDiscoveryResponseTimeoutMs = 5000;
  static constexpr uint32_t kPeerLossGraceMs = 20000;

  // Only an I-Am from the configured device and BACnet/IP endpoint may start
  // a new session. This prevents an unrelated responder from replacing it.
  static bool matchesTarget(uint32_t deviceInstance,
                            const BacnetIpEndpoint& endpoint,
                            uint32_t expectedDeviceInstance,
                            const BacnetIpEndpoint& expectedEndpoint) {
    return deviceInstance == expectedDeviceInstance &&
           endpoint.port == expectedEndpoint.port &&
           endpoint.address[0] == expectedEndpoint.address[0] &&
           endpoint.address[1] == expectedEndpoint.address[1] &&
           endpoint.address[2] == expectedEndpoint.address[2] &&
           endpoint.address[3] == expectedEndpoint.address[3];
  }

  void startInitialDiscovery(uint32_t nowMs) {
    awaitingIAm_ = true;
    discoveryDeadlineMs_ = nowMs + kDiscoveryResponseTimeoutMs;
  }

  // Returns true only for the first request in a recovery cycle. The caller
  // uses that edge to release the old session, scan and subscriptions exactly
  // once, before this controller waits for its bounded retry time.
  bool schedule(BacnetDemoRecoveryReason reason, uint32_t nowMs) {
    if (scheduled_) {
      return false;
    }
    awaitingIAm_ = false;
    scheduled_ = true;
    lastReason_ = reason;
    ++recoveryCount_;
    nextAttemptAtMs_ = nowMs + backoffMs(recoveryCount_ - 1U);
    return true;
  }

  bool due(uint32_t nowMs) const {
    return scheduled_ && reached(nowMs, nextAttemptAtMs_);
  }

  bool beginScheduledDiscovery(uint32_t nowMs) {
    if (!due(nowMs)) {
      return false;
    }
    scheduled_ = false;
    startInitialDiscovery(nowMs);
    return true;
  }

  bool discoveryTimedOut(uint32_t nowMs) const {
    return awaitingIAm_ && reached(nowMs, discoveryDeadlineMs_);
  }

  void acceptIAm() {
    awaitingIAm_ = false;
    peerWasHealthy_ = false;
    peerLossStartedAtMs_ = 0;
  }

  void completeScan(size_t usableObjects, size_t subscriptions) {
    usableObjects_ = usableObjects;
    subscriptionCount_ = subscriptions;
    if (usableObjects == 0 || subscriptions == 0) {
      peerWasHealthy_ = false;
      peerLossStartedAtMs_ = 0;
    }
  }

  // The existing subscription implementation owns individual COV retries.
  // Escalate only after a previously healthy session has lost every active
  // subscription continuously for the grace period.
  bool hasSustainedPeerLoss(uint32_t nowMs,
                            size_t usableObjects,
                            size_t activeSubscriptions) {
    usableObjects_ = usableObjects;
    activeSubscriptions_ = activeSubscriptions;
    if (usableObjects == 0 || subscriptionCount_ == 0) {
      peerLossStartedAtMs_ = 0;
      return false;
    }
    if (activeSubscriptions > 0) {
      peerWasHealthy_ = true;
      peerLossStartedAtMs_ = 0;
      return false;
    }
    if (!peerWasHealthy_) {
      return false;
    }
    if (peerLossStartedAtMs_ == 0) {
      peerLossStartedAtMs_ = nowMs;
      return false;
    }
    return reached(nowMs, peerLossStartedAtMs_ + kPeerLossGraceMs);
  }

  bool scheduled() const {
    return scheduled_;
  }
  bool awaitingIAm() const {
    return awaitingIAm_;
  }
  uint32_t nextAttemptAtMs() const {
    return nextAttemptAtMs_;
  }
  uint32_t recoveryCount() const {
    return recoveryCount_;
  }
  BacnetDemoRecoveryReason lastReason() const {
    return lastReason_;
  }
  size_t usableObjects() const {
    return usableObjects_;
  }
  size_t activeSubscriptions() const {
    return activeSubscriptions_;
  }

private:
  static bool reached(uint32_t nowMs, uint32_t deadlineMs) {
    return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
  }

  static uint32_t backoffMs(uint32_t attemptIndex) {
    static constexpr uint32_t kBackoffMs[] = {5000, 10000, 20000, 30000};
    const size_t index = attemptIndex < sizeof(kBackoffMs) / sizeof(kBackoffMs[0])
                           ? attemptIndex
                           : sizeof(kBackoffMs) / sizeof(kBackoffMs[0]) - 1U;
    return kBackoffMs[index];
  }

  bool scheduled_ = false;
  bool awaitingIAm_ = false;
  bool peerWasHealthy_ = false;
  uint32_t nextAttemptAtMs_ = 0;
  uint32_t discoveryDeadlineMs_ = 0;
  uint32_t peerLossStartedAtMs_ = 0;
  uint32_t recoveryCount_ = 0;
  BacnetDemoRecoveryReason lastReason_ = BacnetDemoRecoveryReason::None;
  size_t usableObjects_ = 0;
  size_t subscriptionCount_ = 0;
  size_t activeSubscriptions_ = 0;
};
