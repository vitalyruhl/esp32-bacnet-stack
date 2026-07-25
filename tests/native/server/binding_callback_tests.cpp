// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetServer.h"

#include <cstdio>
#include <cstring>
#include <limits>

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
  size_t outputCalls = 0;
  size_t reentrantOutputAttempts = 0;
  size_t reentrantOutputRejections = 0;
  bool eventContextsMatchObjectState = true;
  bool reenterFromPresentValue = false;
  BacnetBinaryOutput* output = nullptr;
};

void append(Capture& capture, Event event) {
  if (capture.count < sizeof(capture.events) / sizeof(capture.events[0])) {
    capture.events[capture.count++] = event;
  }
}

void captureOutput(void* context, bool, bool) {
  Capture& capture = *static_cast<Capture*>(context);
  ++capture.outputCalls;
  append(capture, Event::Output);
}

void captureReentrantOutput(void* context, bool, bool) {
  Capture& capture = *static_cast<Capture*>(context);
  ++capture.outputCalls;
  append(capture, Event::Output);
  if (capture.output != nullptr) {
    ++capture.reentrantOutputAttempts;
    if (!capture.output->writeValue(false, 1)) {
      ++capture.reentrantOutputRejections;
    }
  }
}

void capturePriority(void* context, const BacnetPriorityValueChange& change) {
  Capture& capture = *static_cast<Capture*>(context);
  capture.origin = change.origin;
  append(capture, Event::Priority);
  if (capture.output != nullptr) {
    const size_t index = static_cast<size_t>(change.priority - 1U);
    capture.eventContextsMatchObjectState =
      capture.eventContextsMatchObjectState &&
      capture.output->priority.occupied[index] == change.newOccupied &&
      capture.output->priority.slots[index] == change.newValue;
  }
}

void captureEffectivePriority(void* context, const BacnetEffectivePriorityChange& change) {
  Capture& capture = *static_cast<Capture*>(context);
  capture.origin = change.origin;
  append(capture, Event::EffectivePriority);
  if (capture.output != nullptr) {
    capture.eventContextsMatchObjectState =
      capture.eventContextsMatchObjectState &&
      capture.output->priority.effectivePriority() == change.newPriority &&
      capture.output->priority.effectiveValue() == change.newValue;
  }
}

void capturePresentValue(void* context, const BacnetPresentValueChange& change) {
  Capture& capture = *static_cast<Capture*>(context);
  capture.origin = change.origin;
  append(capture, Event::PresentValue);
  if (capture.output != nullptr) {
    capture.eventContextsMatchObjectState =
      capture.eventContextsMatchObjectState &&
      capture.output->priority.effectiveValue() == change.newValue;
  }
  if (capture.reenterFromPresentValue && capture.output != nullptr) {
    capture.reentrantWriteRejected = !capture.output->writeValue(!change.newValue, 1);
  }
}

void captureRelinquish(void* context, const BacnetRelinquishChange& change) {
  Capture& capture = *static_cast<Capture*>(context);
  capture.origin = change.origin;
  append(capture, Event::Relinquish);
  if (capture.output != nullptr) {
    capture.eventContextsMatchObjectState =
      capture.eventContextsMatchObjectState &&
      capture.output->priority.effectivePriority() == change.newEffectivePriority &&
      capture.output->priority.effectiveValue() == change.newEffectiveValue;
  }
}

class TestTransport final : public BacnetDatagramTransport {
public:
  bool begin(uint16_t) override {
    return true;
  }
  void end() override {}

  bool send(const BacnetIpEndpoint&, const uint8_t* data, size_t length) override {
    if (data == nullptr || length > sizeof(lastSent)) {
      return false;
    }
    std::memcpy(lastSent, data, length);
    lastSentLength = length;
    return true;
  }

  size_t receive(uint8_t* data, size_t capacity, BacnetIpEndpoint& source) override {
    if (incomingLength == 0 || incomingLength > capacity) {
      return 0;
    }
    std::memcpy(data, incoming, incomingLength);
    source = incomingSource;
    const size_t result = incomingLength;
    incomingLength = 0;
    return result;
  }

  void idle() override {}

  void queue(const uint8_t* data, size_t length, const BacnetIpEndpoint& source) {
    if (data == nullptr || length > sizeof(incoming)) {
      incomingLength = 0;
      return;
    }
    std::memcpy(incoming, data, length);
    incomingLength = length;
    incomingSource = source;
  }

  uint8_t incoming[BacnetServer::kMaxDatagramSize] = {};
  size_t incomingLength = 0;
  BacnetIpEndpoint incomingSource;
  uint8_t lastSent[BacnetServer::kMaxDatagramSize] = {};
  size_t lastSentLength = 0;
};

struct FloatProvider {
  float value = 0.0F;
};

float readFloat(void* context) {
  return static_cast<FloatProvider*>(context)->value;
}

bool readAnalogInput(TestTransport& transport,
                     BacnetServer& server,
                     BacnetObjectId object,
                     uint8_t invokeId,
                     BacnetValue& value) {
  const BacnetPropertyRequest request{object, BacnetPropertyId::PresentValue, kBacnetNoArrayIndex};
  uint8_t frame[BacnetProtocol::kMaxReadPropertyRequestSize] = {};
  const size_t frameSize = BacnetProtocol::buildReadPropertyRequest(
    frame, sizeof(frame), request, invokeId);
  if (frameSize == 0) {
    return false;
  }
  transport.queue(frame, frameSize, BacnetIpEndpoint(192, 0, 2, 127, 47808));
  return server.poll() == BacnetServerPollResult::ReadPropertyAckSent &&
         BacnetProtocol::parseReadPropertyAck(
           transport.lastSent, transport.lastSentLength, invokeId, request, value);
}

bool readAnalogInputError(TestTransport& transport,
                          BacnetServer& server,
                          BacnetObjectId object,
                          uint8_t invokeId) {
  const BacnetPropertyRequest request{object, BacnetPropertyId::PresentValue, kBacnetNoArrayIndex};
  uint8_t frame[BacnetProtocol::kMaxReadPropertyRequestSize] = {};
  const size_t frameSize = BacnetProtocol::buildReadPropertyRequest(
    frame, sizeof(frame), request, invokeId);
  if (frameSize == 0) {
    return false;
  }
  transport.queue(frame, frameSize, BacnetIpEndpoint(192, 0, 2, 127, 47808));
  BacnetValue error;
  uint32_t errorClass = 0;
  uint32_t errorCode = 0;
  return server.poll() == BacnetServerPollResult::ReadPropertyErrorSent &&
         BacnetProtocol::parseReadPropertyError(transport.lastSent,
                                                transport.lastSentLength,
                                                invokeId,
                                                error,
                                                &errorClass,
                                                &errorCode) &&
         errorClass == 2 && errorCode == 32;
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
  capture.reenterFromPresentValue = true;
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
  const Event effective[] = {Event::Output, Event::Priority, Event::EffectivePriority, Event::PresentValue};
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
  const Event effectiveRelinquish[] = {Event::Output, Event::Priority, Event::EffectivePriority, Event::PresentValue, Event::Relinquish};
  expect(hasOrder(capture, effectiveRelinquish, 5),
         "effective relinquish evaluates output before callbacks");
}

void testOutputBindingReentrancy() {
  BacnetBinaryOutput output;
  BacnetBinaryOutputCallbackStorage callbacks;
  Capture capture;
  capture.output = &output;
  expect(output.configure(312, "Output Binding Reentrancy") ==
           BacnetObjectConfigurationStatus::Ok,
         "output reentrancy configuration");
  output.attachOutput(captureReentrantOutput, &capture);
  output.attachCallbacks(callbacks);
  expect(output.onPriorityValueChange(8, capturePriority, &capture),
         "output reentrancy priority callback binding");
  expect(output.onEffectivePriorityChange(captureEffectivePriority, &capture),
         "output reentrancy effective callback binding");
  expect(output.onPresentValueChange(capturePresentValue, &capture),
         "output reentrancy present callback binding");
  expect(output.onRelinquish(captureRelinquish, &capture),
         "output reentrancy relinquish callback binding");

  expect(output.writeValue(true, 8), "outer write with reentrant output binding");
  const Event writeEvents[] = {Event::Output, Event::Priority, Event::EffectivePriority, Event::PresentValue};
  expect(hasOrder(capture, writeEvents, 4),
         "reentrant output binding produces only the outer callback chain");
  expect(capture.reentrantOutputAttempts == 1 &&
           capture.reentrantOutputRejections == 1 && capture.outputCalls == 1,
         "same-output write from output binding is rejected without another output call");
  expect(output.priority.effectivePriority() == 8 && output.priority.effectiveValue() &&
           !output.priority.occupied[0] && capture.eventContextsMatchObjectState,
         "outer write state and contexts remain authoritative");

  capture.count = 0;
  expect(output.relinquish(8), "outer relinquish with reentrant output binding");
  const Event relinquishEvents[] = {Event::Output, Event::Priority, Event::EffectivePriority, Event::PresentValue, Event::Relinquish};
  expect(hasOrder(capture, relinquishEvents, 5),
         "reentrant output binding leaves relinquish callback order unchanged");
  expect(capture.reentrantOutputAttempts == 2 &&
           capture.reentrantOutputRejections == 2 && capture.outputCalls == 2,
         "reentrant output write adds no second output or callback chain");
  expect(output.priority.effectivePriority() == 0 && !output.priority.effectiveValue() &&
           !output.priority.occupied[0] && capture.eventContextsMatchObjectState,
         "outer relinquish state and contexts remain authoritative");

  capture.count = 0;
  expect(output.writeValue(true, 8), "write succeeds after output binding callback returns");
  expect(output.priority.effectivePriority() == 8 && output.priority.effectiveValue(),
         "reentrancy guard is released after output binding callback returns");
}

class CallbackException {};

struct ThrowingCapture {
  bool throwFromOutput = false;
  bool throwFromPresentValue = false;
  size_t outputCalls = 0;
  size_t presentValueCalls = 0;
};

void throwFromOutput(void* context, bool, bool) {
  ThrowingCapture& capture = *static_cast<ThrowingCapture*>(context);
  ++capture.outputCalls;
  if (capture.throwFromOutput) {
    throw CallbackException{};
  }
}

void throwFromPresentValue(void* context, const BacnetPresentValueChange&) {
  ThrowingCapture& capture = *static_cast<ThrowingCapture*>(context);
  ++capture.presentValueCalls;
  if (capture.throwFromPresentValue) {
    throw CallbackException{};
  }
}

void testCallbackGuardExceptionSafety() {
  BacnetBinaryOutput callbackOutput;
  BacnetBinaryOutputCallbackStorage callbacks;
  ThrowingCapture callbackCapture;
  expect(callbackOutput.configure(313, "Throwing Callback") ==
           BacnetObjectConfigurationStatus::Ok,
         "throwing callback configuration");
  callbackOutput.attachCallbacks(callbacks);
  expect(callbackOutput.onPresentValueChange(throwFromPresentValue, &callbackCapture),
         "throwing present callback binding");
  callbackCapture.throwFromPresentValue = true;
  bool presentExceptionCaught = false;
  try {
    static_cast<void>(callbackOutput.writeValue(true, 8));
  } catch (const CallbackException&) {
    presentExceptionCaught = true;
  }
  expect(presentExceptionCaught && callbackOutput.priority.effectiveValue(),
         "throwing callback leaves the committed state observable");
  callbackCapture.throwFromPresentValue = false;
  expect(callbackOutput.writeValue(false, 8),
         "callback exception does not leave the output reentrancy-locked");
  expect(!callbackOutput.priority.effectiveValue() && callbackCapture.presentValueCalls == 2,
         "normal callback processing resumes after a thrown callback");

  BacnetBinaryOutput outputBinding;
  ThrowingCapture outputCapture;
  expect(outputBinding.configure(314, "Throwing Output Binding") ==
           BacnetObjectConfigurationStatus::Ok,
         "throwing output binding configuration");
  outputBinding.attachOutput(throwFromOutput, &outputCapture);
  outputCapture.throwFromOutput = true;
  bool outputExceptionCaught = false;
  try {
    static_cast<void>(outputBinding.writeValue(true, 8));
  } catch (const CallbackException&) {
    outputExceptionCaught = true;
  }
  expect(outputExceptionCaught && outputBinding.priority.effectiveValue(),
         "throwing output binding leaves the committed state observable");
  outputCapture.throwFromOutput = false;
  expect(outputBinding.writeValue(false, 8),
         "output binding exception does not leave the output reentrancy-locked");
  expect(!outputBinding.priority.effectiveValue() && outputCapture.outputCalls == 2,
         "normal output binding processing resumes after a thrown callback");
}

void testAnalogInputScaleBindingAndFiniteErrors() {
  TestTransport transport;
  BacnetServer server(transport);
  BacnetAnalogInput input;
  FloatProvider provider{2047.5F};
  float directValue = 12.5F;
  const BacnetObjectId object{static_cast<uint16_t>(BacnetObjectType::AnalogInput), 315};
  const BacnetLinearScale scale{0.0F, 4095.0F, 0.0F, 100.0F};
  BacnetServerDevice device;
  device.deviceInstance = 315;
  expect(input.configure(object.instance, "Scaled Input") == BacnetObjectConfigurationStatus::Ok &&
           input.bindInput(readFloat, &provider) == BacnetObjectConfigurationStatus::Ok &&
           input.setInputScale(scale) == BacnetObjectConfigurationStatus::Ok &&
           server.addObject(input) == BacnetObjectConfigurationStatus::Ok && server.begin(device),
         "scaled analog input configuration");

  BacnetValue value;
  expect(readAnalogInput(transport, server, object, 1, value) &&
           value.type == BacnetValueType::Real && value.realValue == 50.0F,
         "public analog input read applies the configured raw scale");
  expect(input.bindPresentValue(&directValue) == BacnetObjectConfigurationStatus::Ok &&
           readAnalogInput(transport, server, object, 2, value) &&
           value.type == BacnetValueType::Real && value.realValue == directValue,
         "bindPresentValue clears raw scaling for the public read path");

  expect(input.bindInput(readFloat, &provider) == BacnetObjectConfigurationStatus::Ok &&
           input.setInputScale(scale) == BacnetObjectConfigurationStatus::Ok,
         "raw scale restored for finite-input failures");
  provider.value = std::numeric_limits<float>::quiet_NaN();
  expect(readAnalogInputError(transport, server, object, 3),
         "NaN raw input produces a deterministic read error");
  provider.value = std::numeric_limits<float>::infinity();
  expect(readAnalogInputError(transport, server, object, 4),
         "positive infinity raw input produces a deterministic read error");
  provider.value = -std::numeric_limits<float>::infinity();
  expect(readAnalogInputError(transport, server, object, 5),
         "negative infinity raw input produces a deterministic read error");

  const BacnetLinearScale nanParameter{
    std::numeric_limits<float>::quiet_NaN(), 1.0F, 0.0F, 1.0F};
  const BacnetLinearScale positiveInfinityParameter{
    0.0F, std::numeric_limits<float>::infinity(), 0.0F, 1.0F};
  const BacnetLinearScale negativeInfinityParameter{
    0.0F, 1.0F, -std::numeric_limits<float>::infinity(), 1.0F};
  expect(input.setInputScale(nanParameter) == BacnetObjectConfigurationStatus::InvalidArgument &&
           input.setInputScale(positiveInfinityParameter) ==
             BacnetObjectConfigurationStatus::InvalidArgument &&
           input.setInputScale(negativeInfinityParameter) ==
             BacnetObjectConfigurationStatus::InvalidArgument,
         "public scale configuration rejects non-finite parameters");
  provider.value = 2047.5F;
  expect(readAnalogInput(transport, server, object, 6, value) &&
           value.type == BacnetValueType::Real && value.realValue == 50.0F,
         "rejected scale parameters retain the prior valid public configuration");
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
  testOutputBindingReentrancy();
  testCallbackGuardExceptionSafety();
  testAnalogInputScaleBindingAndFiniteErrors();
  testBacnetOrigin();
  return failures == 0 ? 0 : 1;
}
