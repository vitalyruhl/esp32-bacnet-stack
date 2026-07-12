// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0
#pragma once

#include "portable/BacnetTypes.h"

#include <cstddef>

struct WindowsIpv4Interface {
  char name[128] = {};
  BacnetIpEndpoint address;
  BacnetIpEndpoint netmask;
};

enum class WindowsInterfaceSelectionStatus { Selected, None, Multiple, NotFound };

BacnetIpEndpoint windowsBroadcastAddress(BacnetIpEndpoint address, BacnetIpEndpoint netmask);
WindowsInterfaceSelectionStatus windowsSelectInterface(const WindowsIpv4Interface* interfaces, size_t count, const BacnetIpEndpoint* requestedAddress, WindowsIpv4Interface& selected);
WindowsInterfaceSelectionStatus windowsDiscoverIpv4Interface(const BacnetIpEndpoint* requestedAddress, WindowsIpv4Interface& selected, WindowsIpv4Interface* candidates, size_t candidateCapacity, size_t& candidateCount);
