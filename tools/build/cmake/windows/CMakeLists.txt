add_library(bacnet_windows STATIC
  ${ESP_BACNET_ROOT}/src/platform/windows/WindowsSocketRuntime.cpp
  ${ESP_BACNET_ROOT}/src/platform/windows/WindowsBacnetDatagramTransport.cpp
  ${ESP_BACNET_ROOT}/src/platform/windows/WindowsMonotonicClock.cpp
  ${ESP_BACNET_ROOT}/src/platform/windows/WindowsConsoleLogSink.cpp)
target_compile_features(bacnet_windows PUBLIC cxx_std_17)
target_include_directories(bacnet_windows PUBLIC
  ${ESP_BACNET_ROOT}/src)
target_link_libraries(bacnet_windows PUBLIC bacnet_client ws2_32)

add_library(bacnet_cli_common STATIC
  ${ESP_BACNET_ROOT}/examples/windows/bacnet-cli/src/BacnetCliSupport.cpp
  ${ESP_BACNET_ROOT}/examples/windows/bacnet-cli/src/BacnetNativeCli.cpp
  ${ESP_BACNET_ROOT}/examples/windows/bacnet-cli/src/WindowsInterfaceSelection.cpp)
target_compile_features(bacnet_cli_common PUBLIC cxx_std_17)
target_include_directories(bacnet_cli_common PUBLIC
  ${ESP_BACNET_ROOT}/src
  ${ESP_BACNET_ROOT}/examples/windows/bacnet-cli/src)
target_link_libraries(bacnet_cli_common PUBLIC bacnet_windows iphlpapi)

add_executable(bacnet-discover
  ${ESP_BACNET_ROOT}/examples/windows/bacnet-cli/src/bacnet_discover.cpp)
target_compile_features(bacnet-discover PRIVATE cxx_std_17)
target_link_libraries(bacnet-discover PRIVATE bacnet_cli_common bacnet_client)

add_executable(bacnet-client
  ${ESP_BACNET_ROOT}/examples/windows/bacnet-cli/src/bacnet_client.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetDeviceSession.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetRemoteObject.cpp)
target_compile_features(bacnet-client PRIVATE cxx_std_17)
target_compile_definitions(bacnet-client PRIVATE
  ESP_BACNET_ENABLE_WRITE_PROPERTY=1
  ESP_BACNET_ENABLE_PRIORITY_WRITE=1)
target_link_libraries(bacnet-client PRIVATE bacnet_cli_common bacnet_client_priority_enabled)

add_executable(bacnet-write-hil
  ${ESP_BACNET_ROOT}/tests/hil/windows/bacnet-write-hil/bacnet_write_hil.cpp
  ${ESP_BACNET_ROOT}/tests/hil/windows/bacnet-write-hil/BacnetWriteHil.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetDeviceSession.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetRemoteObject.cpp)
target_compile_features(bacnet-write-hil PRIVATE cxx_std_17)
target_compile_definitions(bacnet-write-hil PRIVATE
  ESP_BACNET_ENABLE_WRITE_PROPERTY=1
  ESP_BACNET_ENABLE_PRIORITY_WRITE=1)
target_include_directories(bacnet-write-hil PRIVATE
  ${ESP_BACNET_ROOT}/tests/hil/esp32/shared)
target_link_libraries(bacnet-write-hil PRIVATE
  bacnet_client_priority_enabled bacnet_cli_common bacnet_windows)

add_executable(bacnet_cli_smoke
  ${ESP_BACNET_ROOT}/tests/windows/cli-smoke/bacnet_cli_smoke.cpp)
target_compile_features(bacnet_cli_smoke PRIVATE cxx_std_17)
target_link_libraries(bacnet_cli_smoke PRIVATE
  bacnet_cli_common bacnet_client bacnet_windows)

add_executable(bacnet-discover-smoke
  ${ESP_BACNET_ROOT}/tests/windows/cli-smoke/bacnet_discover_smoke.cpp)
target_compile_features(bacnet-discover-smoke PRIVATE cxx_std_17)
target_link_libraries(bacnet-discover-smoke PRIVATE
  bacnet_cli_common bacnet_client bacnet_windows)

add_executable(bacnet_windows_tests ${ESP_BACNET_ROOT}/tests/windows/windows_transport_tests.cpp)
target_compile_features(bacnet_windows_tests PRIVATE cxx_std_17)
target_link_libraries(bacnet_windows_tests PRIVATE bacnet_windows)

add_executable(bacnet_server_client_loopback_tests
  ${ESP_BACNET_ROOT}/tests/windows/server_client_loopback_tests.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetDeviceSession.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetRemoteObject.cpp)
target_compile_features(bacnet_server_client_loopback_tests PRIVATE cxx_std_17)
target_link_libraries(bacnet_server_client_loopback_tests PRIVATE
  bacnet_client bacnet_server bacnet_windows)

add_executable(bacnet_binding_callback_tests
  ${ESP_BACNET_ROOT}/tests/native/server/binding_callback_tests.cpp)
target_compile_features(bacnet_binding_callback_tests PRIVATE cxx_std_17)
target_link_libraries(bacnet_binding_callback_tests PRIVATE bacnet_server)

add_executable(bacnet_cli_support_tests ${ESP_BACNET_ROOT}/tests/windows/cli_support_tests.cpp)
target_compile_features(bacnet_cli_support_tests PRIVATE cxx_std_17)
target_link_libraries(bacnet_cli_support_tests PRIVATE bacnet_cli_common)

add_executable(bacnet_subscribe_cov_protocol_tests
  ${ESP_BACNET_ROOT}/tests/native/protocol/subscribe_cov_protocol_tests.cpp)
target_compile_features(bacnet_subscribe_cov_protocol_tests PRIVATE cxx_std_17)
target_link_libraries(bacnet_subscribe_cov_protocol_tests PRIVATE bacnet_portable)

add_executable(bacnet_device_read_protocol_tests
  ${ESP_BACNET_ROOT}/tests/native/protocol/device_read_protocol_tests.cpp)
target_compile_features(bacnet_device_read_protocol_tests PRIVATE cxx_std_17)
target_link_libraries(bacnet_device_read_protocol_tests PRIVATE bacnet_portable)

add_library(bacnet_client_write_enabled STATIC
  ${ESP_BACNET_ROOT}/src/core/client/BacnetClient.cpp
  ${ESP_BACNET_ROOT}/src/support/BacnetLogger.cpp)
target_compile_features(bacnet_client_write_enabled PUBLIC cxx_std_17)
target_compile_definitions(bacnet_client_write_enabled PRIVATE ESP_BACNET_ENABLE_WRITE_PROPERTY=1)
target_include_directories(bacnet_client_write_enabled PUBLIC ${ESP_BACNET_ROOT}/src)
target_link_libraries(bacnet_client_write_enabled PUBLIC bacnet_portable)

add_library(bacnet_client_priority_enabled STATIC
  ${ESP_BACNET_ROOT}/src/core/client/BacnetClient.cpp
  ${ESP_BACNET_ROOT}/src/support/BacnetLogger.cpp)
target_compile_features(bacnet_client_priority_enabled PUBLIC cxx_std_17)
target_compile_definitions(bacnet_client_priority_enabled PRIVATE
  ESP_BACNET_ENABLE_WRITE_PROPERTY=1
  ESP_BACNET_ENABLE_PRIORITY_WRITE=1)
target_include_directories(bacnet_client_priority_enabled PUBLIC ${ESP_BACNET_ROOT}/src)
target_link_libraries(bacnet_client_priority_enabled PUBLIC bacnet_portable)

add_executable(bacnet_write_property_tests
  ${ESP_BACNET_ROOT}/tests/native/client/write_property_tests.cpp)
target_compile_features(bacnet_write_property_tests PRIVATE cxx_std_17)
target_compile_definitions(bacnet_write_property_tests PRIVATE
  ESP_BACNET_ENABLE_WRITE_PROPERTY=1
  ESP_BACNET_ENABLE_PRIORITY_WRITE=1)
target_link_libraries(bacnet_write_property_tests PRIVATE bacnet_client_priority_enabled)

add_executable(bacnet_write_property_disabled_tests
  ${ESP_BACNET_ROOT}/tests/native/client/write_property_disabled_tests.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetDeviceSession.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetRemoteObject.cpp)
target_compile_features(bacnet_write_property_disabled_tests PRIVATE cxx_std_17)
target_link_libraries(bacnet_write_property_disabled_tests PRIVATE bacnet_client)

add_executable(bacnet_priority_write_disabled_tests
  ${ESP_BACNET_ROOT}/tests/native/client/priority_write_disabled_tests.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetDeviceSession.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetRemoteObject.cpp)
target_compile_features(bacnet_priority_write_disabled_tests PRIVATE cxx_std_17)
target_compile_definitions(bacnet_priority_write_disabled_tests PRIVATE
  ESP_BACNET_ENABLE_WRITE_PROPERTY=1
  ESP_BACNET_ENABLE_PRIORITY_WRITE=0)
target_link_libraries(bacnet_priority_write_disabled_tests PRIVATE bacnet_client_write_enabled)

add_executable(bacnet_priority_helpers_tests
  ${ESP_BACNET_ROOT}/tests/native/client/priority_helpers_tests.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetClient.cpp
  ${ESP_BACNET_ROOT}/src/support/BacnetLogger.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetDeviceSession.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetRemoteObject.cpp)
target_compile_features(bacnet_priority_helpers_tests PRIVATE cxx_std_17)
target_compile_definitions(bacnet_priority_helpers_tests PRIVATE
  ESP_BACNET_ENABLE_WRITE_PROPERTY=1
  ESP_BACNET_ENABLE_PRIORITY_WRITE=1)
target_include_directories(bacnet_priority_helpers_tests PRIVATE
  ${ESP_BACNET_ROOT}/src)
target_link_libraries(bacnet_priority_helpers_tests PRIVATE bacnet_portable)

add_executable(bacnet_demo_binary_value_status_tests
  ${ESP_BACNET_ROOT}/tests/native/examples/client-demo/demo_binary_value_status_tests.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetDeviceSession.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetRemoteObject.cpp)
target_compile_features(bacnet_demo_binary_value_status_tests PRIVATE cxx_std_17)
target_include_directories(bacnet_demo_binary_value_status_tests PRIVATE
  ${ESP_BACNET_ROOT}/examples/esp32/shared/client-demo/src)
target_link_libraries(bacnet_demo_binary_value_status_tests PRIVATE bacnet_client)

add_executable(bacnet_demo_property_browser_tests
  ${ESP_BACNET_ROOT}/tests/native/examples/client-demo/demo_property_browser_tests.cpp
  ${ESP_BACNET_ROOT}/examples/esp32/shared/client-demo/src/BacnetDemoPropertyBrowser.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetDeviceSession.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetRemoteObject.cpp)
target_compile_features(bacnet_demo_property_browser_tests PRIVATE cxx_std_17)
target_include_directories(bacnet_demo_property_browser_tests PRIVATE
  ${ESP_BACNET_ROOT}/examples/esp32/shared/client-demo/src)
target_link_libraries(bacnet_demo_property_browser_tests PRIVATE bacnet_client)

add_executable(bacnet_object_list_scan_tests
  ${ESP_BACNET_ROOT}/tests/native/client/object_list_scan_tests.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetDeviceSession.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetRemoteObject.cpp)
target_compile_features(bacnet_object_list_scan_tests PRIVATE cxx_std_17)
target_link_libraries(bacnet_object_list_scan_tests PRIVATE bacnet_client)

add_executable(bacnet_write_hil_tests
  ${ESP_BACNET_ROOT}/tests/hil/native/write_hil_tests.cpp
  ${ESP_BACNET_ROOT}/tests/hil/windows/bacnet-write-hil/BacnetWriteHil.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetClient.cpp
  ${ESP_BACNET_ROOT}/src/support/BacnetLogger.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetDeviceSession.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetRemoteObject.cpp)
target_compile_features(bacnet_write_hil_tests PRIVATE cxx_std_17)
target_compile_definitions(bacnet_write_hil_tests PRIVATE
  ESP_BACNET_ENABLE_WRITE_PROPERTY=1
  ESP_BACNET_ENABLE_PRIORITY_WRITE=1)
target_include_directories(bacnet_write_hil_tests PRIVATE
  ${ESP_BACNET_ROOT}/src
  ${ESP_BACNET_ROOT}/tests/hil/windows/bacnet-write-hil
  ${ESP_BACNET_ROOT}/examples/common)
target_link_libraries(bacnet_write_hil_tests PRIVATE bacnet_portable)

if(BUILD_TESTING)
  add_test(NAME bacnet_windows_tests COMMAND bacnet_windows_tests)
  add_test(NAME bacnet_server_client_loopback_tests
    COMMAND bacnet_server_client_loopback_tests)
  add_test(NAME bacnet_binding_callback_tests COMMAND bacnet_binding_callback_tests)
  add_test(NAME bacnet_cli_support_tests COMMAND bacnet_cli_support_tests)
  add_test(NAME bacnet_subscribe_cov_protocol_tests
    COMMAND bacnet_subscribe_cov_protocol_tests)
  add_test(NAME bacnet_device_read_protocol_tests
    COMMAND bacnet_device_read_protocol_tests)
  add_test(NAME bacnet_write_property_tests COMMAND bacnet_write_property_tests)
  add_test(NAME bacnet_write_property_disabled_tests COMMAND bacnet_write_property_disabled_tests)
  add_test(NAME bacnet_priority_write_disabled_tests COMMAND bacnet_priority_write_disabled_tests)
  add_test(NAME bacnet_priority_helpers_tests COMMAND bacnet_priority_helpers_tests)
  add_test(NAME bacnet_demo_binary_value_status_tests
    COMMAND bacnet_demo_binary_value_status_tests)
  add_test(NAME bacnet_demo_property_browser_tests
    COMMAND bacnet_demo_property_browser_tests)
  add_test(NAME bacnet_object_list_scan_tests COMMAND bacnet_object_list_scan_tests)
  if(WIN32)
    find_program(BACNET_POWERSHELL_EXECUTABLE NAMES powershell pwsh)
    if(BACNET_POWERSHELL_EXECUTABLE)
      add_test(NAME bacnet_client_demo_parity
        COMMAND ${BACNET_POWERSHELL_EXECUTABLE}
          -NoProfile
          -ExecutionPolicy Bypass
          -File ${ESP_BACNET_ROOT}/tools/check-client-demo-parity.ps1)
    endif()
  endif()
  add_test(NAME bacnet_write_hil_tests COMMAND bacnet_write_hil_tests)
  add_test(NAME bacnet_cli_help COMMAND bacnet_cli_smoke --help)
  add_test(NAME bacnet_cli_self_test COMMAND bacnet_cli_smoke --self-test)
  add_test(NAME bacnet_cli_unknown_option COMMAND bacnet_cli_smoke --unknown)
  set_tests_properties(bacnet_cli_unknown_option PROPERTIES WILL_FAIL TRUE)
  add_test(NAME bacnet_discover_help COMMAND bacnet-discover-smoke --help)
  add_test(NAME bacnet_discover_self_test COMMAND bacnet-discover-smoke --self-test)
  add_test(NAME bacnet_discover_invalid_arguments COMMAND bacnet-discover-smoke --bind 127.0.0.1)
  set_tests_properties(bacnet_discover_invalid_arguments PROPERTIES WILL_FAIL TRUE)
  add_test(NAME bacnet_discover_product_help COMMAND bacnet-discover --help)
  add_test(NAME bacnet_client_help COMMAND bacnet-client --help)
  add_test(NAME bacnet_client_list_help COMMAND bacnet-client list --help)
  add_test(NAME bacnet_client_read_help COMMAND bacnet-client read --help)
  add_test(NAME bacnet_client_subscribe_invalid_type COMMAND bacnet-client --bind 127.0.0.1 --target 127.0.0.1 --device-id 1 subscribe BV1)
  set_tests_properties(bacnet_client_subscribe_invalid_type PROPERTIES WILL_FAIL TRUE)
  add_test(NAME bacnet_client_priority_invalid_instance COMMAND bacnet-client --bind 127.0.0.1 --target 127.0.0.1 --device-id 1 priority-slot BV4194304 --priority 8)
  set_tests_properties(bacnet_client_priority_invalid_instance PROPERTIES WILL_FAIL TRUE)
  add_test(NAME bacnet_client_priority_invalid_priority COMMAND bacnet-client --bind 127.0.0.1 --target 127.0.0.1 --device-id 1 priority-slot BV1 --priority 0)
  set_tests_properties(bacnet_client_priority_invalid_priority PROPERTIES WILL_FAIL TRUE)
  add_test(NAME bacnet_client_priority_missing_execute COMMAND bacnet-client --bind 127.0.0.1 --target 127.0.0.1 --device-id 1 write-bv-priority BV1 active --priority 8)
  set_tests_properties(bacnet_client_priority_missing_execute PROPERTIES WILL_FAIL TRUE)
  add_test(NAME bacnet_write_hil_help COMMAND bacnet-write-hil --help)
endif()
