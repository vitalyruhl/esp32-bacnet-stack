// SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0

#pragma once

#ifndef ESP_BACNET_ENABLE_WRITE_PROPERTY
#define ESP_BACNET_ENABLE_WRITE_PROPERTY 0
#endif

#ifndef ESP_BACNET_ENABLE_PRIORITY_WRITE
#define ESP_BACNET_ENABLE_PRIORITY_WRITE 0
#endif

#if ESP_BACNET_ENABLE_PRIORITY_WRITE && !ESP_BACNET_ENABLE_WRITE_PROPERTY
#error "ESP_BACNET_ENABLE_PRIORITY_WRITE requires ESP_BACNET_ENABLE_WRITE_PROPERTY"
#endif

inline constexpr bool bacnetWritePropertyEnabled(bool hasPriority = false) {
#if !ESP_BACNET_ENABLE_WRITE_PROPERTY
  (void)hasPriority;
  return false;
#elif !ESP_BACNET_ENABLE_PRIORITY_WRITE
  return !hasPriority;
#else
  (void)hasPriority;
  return true;
#endif
}
