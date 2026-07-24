// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetServer.h"

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

enum class Event : uint8_t {
  Output,
  Priority,
  EffectivePriority,
  PresentValue,
  Relinquish,
};

struct Capture {
  Event events[16] = {};
  size_t count = 0;
  BacnetChangeOrigin origin = BacnetChangeOrigin::Local;
  bool reentrantWriteRejected = false;
  BacnetBinaryOutput* output = nullptr;
};

void append(Capture& capture, Event event) {
  if (capture.count < sizeof(capture.events) / sizeof(capture.events[0])) {
    capture.events[capture.count++] = event;
  }
}

void captureOutput(void* context, bool, bool) {
  append(*static_cast<Capture*>(context), Event::Output);
}

void capturePriority(void* context, const BacnetPriorityValueChange& change) {
  Capture& capture = *static_cast<Capture*>(context);
  capture.origin = change.origin;
  append(capture, Event::Priority);
}

void captureEffectivePriority(void* context, const BacnetEffectivePriorityChange& change) {
  Capture& capture = *static_cast<Capture*>(context);
  capture.origin = change.origin;
  append(capture, Event::EffectivePriority);
}

void capturePresentValue(void* context, const BacnetPresentValueChange& change) {
  Capture& capture = *static_cast<Capture*>(context);
  capture.origin = change.origin;
  append(capture, Event::PresentValue);
  if (capture.output != nullptr) {
    capture.reentrantWriteRejected = !capture.output->writeValue(!change.newValue, 1);
  }
}

void captureRelinquish(void* context, const BacnetRelinquishChange& change) {
  Capture& capture = *static_cast<Capture*>(context);
  capture.origin = change.origin;
  append(capture, Event::Relinquish);
}

bool hasOrder(const Capture& capture,
              const Event* expected,
              size_t expectedCount) {
  if (capture.count != expectedCount) {
    return false;
  }
  for (size_t index = 0; index < expectedCount; ++index) {
    if (capture.events[index] != expected[index]) {
      return false;
    }
  }
  return true;
}

void testLinearScale() {
  float value = 0.0F;
  const BacnetLinearScale percent{0.0F, 4095.0F, 0.0F, 100.0F};
  expect(percent.apply(0.0F, value) && value == 0.0F, "scale lower bound");
  expect(percent.apply(2047.5F, value) && value == 50.0F, "scale midpoint");
  expect(percent.apply(4095.0F, value) && value == 100.0F, "scale upper bound");
  expect(percent.apply(-1.0F, value) && value == 0.0F, "scale clamps below raw range");
  expect(percent.apply(5000.0F, value) && value == 100.0F, "scale clamps above raw range");

  const BacnetLinearScale reversed{100.0F, 0.0F, 0.0F, 1.0F};
  expect(reversed.apply(100.0F, value) && value == 0.0F, "scale supports reversed raw range");
  expect(reversed.apply(0.0F, value) && value == 1.0F, "scale reverses raw endpoint");

  const BacnetLinearScale degenerate{1.0F, 1.0F, 0.0F, 1.0F};
  value = 12.0F;
  expect(!degenerate.apply(1.0F, value) && value == 0.0F,
         "scale rejects degenerate raw range deterministically");
}

void testCallbackOrderAndReentrancy() {
  BacnetBinaryOutput output;
  BacnetBinaryOutputCallbackStorage callbacks;
  Capture capture;
  capture.output = &output;
  expect(output.configure(310, "Callback Output") == BacnetObjectConfigurationStatus::Ok,
         "output configuration");
  output.attachOutput(captureOutput, &capture);
  output.attachCallbacks(callbacks);
  expect(output.onPriorityValueChange(8, capturePriority, &capture), "priority callback binding");
  expect(output.onPriorityValueChange(16, capturePriority, &capture), "hidden priority callback binding");
  expect(output.onEffectivePriorityChange(captureEffectivePriority, &capture),
         "effective priority callback binding");
  expect(output.onPresentValueChange(capturePresentValue, &capture),
         "present value callback binding");
  expect(output.onRelinquish(captureRelinquish, &capture), "relinquish callback binding");

  expect(output.writeValue(false, 16), "initial priority write");
  const Event initial[] = {Event::Priority, Event::EffectivePriority};
  expect(hasOrder(capture, initial, 2), "same-value priority change has no output or present callback");

  capture.count = 0;
  expect(output.writeValue(true, 8), "effective priority write");
  const Event effective[] = {Event::Output, Event::Priority, Event::EffectivePriority,
                             Event::PresentValue};
  expect(hasOrder(capture, effective, 4), "output precedes specialized callbacks");
  expect(capture.reentrantWriteRejected, "same-object reentrant write is rejected");

  capture.count = 0;
  capture.reentrantWriteRejected = false;
  expect(output.writeValue(true, 16), "hidden slot value write");
  const Event hiddenWrite[] = {Event::Priority};
  expect(hasOrder(capture, hiddenWrite, 1), "hidden slot write does not change effective value");

  capture.count = 0;
  expect(output.relinquish(16), "hidden slot relinquish");
  const Event hiddenRelinquish[] = {Event::Priority, Event::Relinquish};
  expect(hasOrder(capture, hiddenRelinquish, 2),
         "hidden slot relinquish reports no present-value change");

  capture.count = 0;
  expect(output.relinquish(8), "effective slot relinquish");
  const Event effectiveRelinquish[] = {Event::Output, Event::Priority,
                                       Event::EffectivePriority, Event::PresentValue,
                                       Event::Relinquish};
  expect(hasOrder(capture, effectiveRelinquish, 5),
         "effective relinquish evaluates output before callbacks");
}

void testBacnetOrigin() {
  BacnetServerBinaryOutput output;
  BacnetBinaryOutputCallbackStorage callbacks;
  Capture capture;
  output.instance = 311;
  output.callbackStorage = &callbacks;
  callbacks.presentValueChange = capturePresentValue;
  callbacks.presentValueContext = &capture;
  expect(output.applyPriorityValue(true, 8, false, BacnetChangeOrigin::BacnetWriteProperty),
         "BACnet priority write");
  expect(capture.origin == BacnetChangeOrigin::BacnetWriteProperty,
         "BACnet origin reaches callback");
}

} // namespace

int main() {
  testLinearScale();
  testCallbackOrderAndReentrancy();
  testBacnetOrigin();
  return failures == 0 ? 0 : 1;
}
