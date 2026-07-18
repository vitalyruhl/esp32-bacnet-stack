// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#include "BacnetServer.h"

BacnetServer::BacnetServer(BacnetDatagramTransport& transport)
    : transport_(&transport) {}

bool BacnetServer::setTransport(BacnetDatagramTransport& transport) {
  if (running_) {
    return false;
  }
  transport_ = &transport;
  return true;
}

bool BacnetServer::begin(const BacnetServerDevice& configuredDevice,
                         uint16_t portValue) {
  if (transport_ == nullptr ||
      configuredDevice.deviceInstance > 0x003FFFFFUL ||
      !transport_->begin(portValue)) {
    return false;
  }

  device_ = configuredDevice;
  port_ = portValue;
  running_ = true;
  return true;
}

bool BacnetServer::begin(uint32_t deviceInstanceValue, uint16_t portValue) {
  BacnetServerDevice configuredDevice;
  configuredDevice.deviceInstance = deviceInstanceValue;
  return begin(configuredDevice, portValue);
}

void BacnetServer::end() {
  if (running_ && transport_ != nullptr) {
    transport_->end();
  }
  running_ = false;
}

bool BacnetServer::isRunning() const {
  return running_;
}

const BacnetServerDevice& BacnetServer::device() const {
  return device_;
}

uint32_t BacnetServer::deviceInstance() const {
  return device_.deviceInstance;
}

uint16_t BacnetServer::port() const {
  return port_;
}

BacnetServerPollResult BacnetServer::poll() {
  if (!running_ || transport_ == nullptr) {
    return BacnetServerPollResult::NotRunning;
  }

  uint8_t packet[kMaxDatagramSize] = {};
  BacnetIpEndpoint source;
  const size_t bytesRead = transport_->receive(packet, sizeof(packet), source);
  if (bytesRead == 0) {
    return BacnetServerPollResult::NoDatagram;
  }
  if (bytesRead > sizeof(packet)) {
    return BacnetServerPollResult::Malformed;
  }

  BacnetWhoIsRequest whoIs;
  if (BacnetProtocol::parseWhoIsRequest(packet, bytesRead, whoIs)) {
    if (!whoIs.includes(device_.deviceInstance)) {
      return BacnetServerPollResult::Ignored;
    }
    return sendIAm(source) ? BacnetServerPollResult::IAmSent
                           : BacnetServerPollResult::SendFailed;
  }

  BacnetConfirmedRequestHeader confirmedRequest;
  const auto confirmedStatus = BacnetProtocol::parseConfirmedRequestHeader(
    packet, bytesRead, confirmedRequest);
  if (confirmedStatus == BacnetConfirmedRequestParseStatus::Malformed) {
    return BacnetServerPollResult::Malformed;
  }
  if (confirmedStatus != BacnetConfirmedRequestParseStatus::Confirmed) {
    return BacnetServerPollResult::Ignored;
  }

  return sendReject(source, confirmedRequest.invokeId)
           ? BacnetServerPollResult::RejectSent
           : BacnetServerPollResult::SendFailed;
}

bool BacnetServer::sendIAm(const BacnetIpEndpoint& destination) {
  uint8_t response[BacnetProtocol::kMaxIAmResponseSize] = {};
  const BacnetIAmDeviceInfo deviceInfo{
    device_.deviceInstance,
    device_.maxApduLengthAccepted,
    device_.segmentationSupported,
    device_.vendorId,
  };
  const size_t responseSize =
    BacnetProtocol::buildIAmResponse(response, sizeof(response), deviceInfo);
  return responseSize != 0 && transport_->send(destination, response, responseSize);
}

bool BacnetServer::sendReject(const BacnetIpEndpoint& destination,
                              uint8_t invokeId) {
  uint8_t response[BacnetProtocol::kRejectResponseSize] = {};
  const size_t responseSize = BacnetProtocol::buildRejectResponse(
    response, sizeof(response), invokeId, kRejectReasonUnrecognizedService);
  return responseSize != 0 && transport_->send(destination, response, responseSize);
}
