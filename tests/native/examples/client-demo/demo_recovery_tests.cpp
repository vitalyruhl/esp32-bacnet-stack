// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetDemoRecovery.h"

#include <cstdio>

namespace {

int failures = 0;

bool expect(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "[E] %s\n", message);
    ++failures;
  }
  return condition;
}

constexpr BacnetIpEndpoint kTarget(192, 168, 2, 126, 47808);
constexpr uint32_t kTargetDevice = 1682127;

void testEmptyScanSchedulesOneCleanRecovery() {
  BacnetDemoRecovery recovery;
  recovery.startInitialDiscovery(0);
  recovery.acceptIAm();
  recovery.completeScan(0, 0);

  expect(recovery.schedule(BacnetDemoRecoveryReason::EmptyObjectList, 100),
         "an empty terminal scan must schedule one recovery cycle");
  expect(!recovery.schedule(BacnetDemoRecoveryReason::EmptyObjectList, 101),
         "a pending recovery must not request cleanup repeatedly");
  expect(recovery.recoveryCount() == 1,
         "an empty scan must increment the recovery counter once");
  expect(!recovery.due(5099), "recovery must wait for its bounded backoff");
  expect(recovery.due(5100), "recovery must become due after the first backoff");
  expect(recovery.beginScheduledDiscovery(5100),
         "a due recovery must start a fresh discovery attempt");
  expect(recovery.awaitingIAm(), "fresh discovery must await a matching I-Am");
}

void testMatchingIAmAndEightObjectsRestoreSession() {
  BacnetDemoRecovery recovery;
  expect(BacnetDemoRecovery::matchesTarget(kTargetDevice, kTarget, kTargetDevice, kTarget),
         "the configured device and endpoint must be accepted");
  expect(!BacnetDemoRecovery::matchesTarget(kTargetDevice + 1, kTarget, kTargetDevice, kTarget),
         "an I-Am from another device ID must be ignored");
  expect(!BacnetDemoRecovery::matchesTarget(
           kTargetDevice, BacnetIpEndpoint(192, 168, 2, 125, 47808), kTargetDevice, kTarget),
         "an I-Am from another endpoint must be ignored");

  recovery.startInitialDiscovery(0);
  recovery.acceptIAm();
  recovery.completeScan(8, 8);
  expect(recovery.usableObjects() == 8,
         "a recovered scan must retain all eight live-COV objects");
  recovery.hasSustainedPeerLoss(1, 8, 8);
  expect(recovery.activeSubscriptions() == 8,
         "a recovered scan must restore all eight COV subscriptions");
}

void testSubscriptionRetryPrecedesPeerRecovery() {
  BacnetDemoRecovery recovery;
  recovery.completeScan(8, 8);
  expect(!recovery.hasSustainedPeerLoss(0, 8, 8),
         "a healthy COV session must not recover");
  expect(!recovery.hasSustainedPeerLoss(1000, 8, 7),
         "a single COV timeout must retain the normal per-subscription retry");
  expect(!recovery.hasSustainedPeerLoss(2000, 8, 0),
         "total loss starts the grace interval before recovery");
  expect(!recovery.hasSustainedPeerLoss(2000 + BacnetDemoRecovery::kPeerLossGraceMs - 1, 8, 0),
         "the peer-loss grace interval must remain bounded");
  expect(recovery.hasSustainedPeerLoss(2000 + BacnetDemoRecovery::kPeerLossGraceMs, 8, 0),
         "sustained total loss must eventually request recovery");
  expect(recovery.schedule(BacnetDemoRecoveryReason::PeerSubscriptionsLost,
                           2000 + BacnetDemoRecovery::kPeerLossGraceMs),
         "sustained total loss must schedule a recovery cycle");
}

void testLocalIpChangeUsesControlledRecoveryAndStablePeerDoesNotLoop() {
  BacnetDemoRecovery recovery;
  expect(recovery.schedule(BacnetDemoRecoveryReason::LocalIpChanged, 0),
         "a local IP change must schedule controlled transport recovery");
  expect(recovery.lastReason() == BacnetDemoRecoveryReason::LocalIpChanged,
         "local IP diagnostics must retain the recovery reason");
  expect(!recovery.schedule(BacnetDemoRecoveryReason::LocalIpChanged, 1),
         "a local IP change must not create a loop per main iteration");

  BacnetDemoRecovery stable;
  stable.completeScan(8, 8);
  for (uint32_t now = 0; now < 120000; now += 1000) {
    expect(!stable.hasSustainedPeerLoss(now, 8, 8),
           "a stable COV session must not schedule discovery loops");
  }
  expect(stable.recoveryCount() == 0,
         "a stable COV session must keep the recovery counter unchanged");
}

} // namespace

int main() {
  testEmptyScanSchedulesOneCleanRecovery();
  testMatchingIAmAndEightObjectsRestoreSession();
  testSubscriptionRetryPrecedesPeerRecovery();
  testLocalIpChangeUsesControlledRecoveryAndStablePeerDoesNotLoop();
  return failures == 0 ? 0 : 1;
}
