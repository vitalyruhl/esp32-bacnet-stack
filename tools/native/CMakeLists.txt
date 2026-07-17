add_library(bacnet_windows STATIC
  ${CMAKE_CURRENT_LIST_DIR}/../../src/platform/windows/WindowsSocketRuntime.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/platform/windows/WindowsBacnetDatagramTransport.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/platform/windows/WindowsMonotonicClock.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/platform/windows/WindowsConsoleLogSink.cpp)
target_compile_features(bacnet_windows PUBLIC cxx_std_17)
target_include_directories(bacnet_windows PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_link_libraries(bacnet_windows PUBLIC bacnet_client ws2_32)

add_library(bacnet_cli_common STATIC
  ${CMAKE_CURRENT_LIST_DIR}/cli/BacnetCliSupport.cpp
  ${CMAKE_CURRENT_LIST_DIR}/cli/BacnetNativeCli.cpp
  ${CMAKE_CURRENT_LIST_DIR}/cli/WindowsInterfaceSelection.cpp)
target_compile_features(bacnet_cli_common PUBLIC cxx_std_17)
target_include_directories(bacnet_cli_common PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../../src
  ${CMAKE_CURRENT_LIST_DIR}/cli)
target_link_libraries(bacnet_cli_common PUBLIC bacnet_windows iphlpapi)

add_executable(bacnet-discover cli/bacnet_discover.cpp)
target_compile_features(bacnet-discover PRIVATE cxx_std_17)
target_link_libraries(bacnet-discover PRIVATE bacnet_cli_common bacnet_client)

add_executable(bacnet-client
  cli/bacnet_client.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetDeviceSession.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetRemoteObject.cpp)
target_compile_features(bacnet-client PRIVATE cxx_std_17)
target_compile_definitions(bacnet-client PRIVATE
  ESP_BACNET_ENABLE_WRITE_PROPERTY=1
  ESP_BACNET_ENABLE_PRIORITY_WRITE=1)
target_link_libraries(bacnet-client PRIVATE bacnet_cli_common bacnet_client_priority_enabled)

add_executable(bacnet-write-hil
  cli/bacnet_write_hil.cpp
  cli/BacnetWriteHil.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetDeviceSession.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetRemoteObject.cpp)
target_compile_features(bacnet-write-hil PRIVATE cxx_std_17)
target_compile_definitions(bacnet-write-hil PRIVATE
  ESP_BACNET_ENABLE_WRITE_PROPERTY=1
  ESP_BACNET_ENABLE_PRIORITY_WRITE=1)
target_link_libraries(bacnet-write-hil PRIVATE
  bacnet_client_priority_enabled bacnet_cli_common bacnet_windows)

add_executable(bacnet_cli_smoke cli/bacnet_cli_smoke.cpp)
target_compile_features(bacnet_cli_smoke PRIVATE cxx_std_17)
target_link_libraries(bacnet_cli_smoke PRIVATE
  bacnet_cli_common bacnet_client bacnet_windows)

add_executable(bacnet-discover-smoke cli/bacnet_discover_smoke.cpp)
target_compile_features(bacnet-discover-smoke PRIVATE cxx_std_17)
target_link_libraries(bacnet-discover-smoke PRIVATE
  bacnet_cli_common bacnet_client bacnet_windows)

add_executable(bacnet_windows_tests test/windows_transport_tests.cpp)
target_compile_features(bacnet_windows_tests PRIVATE cxx_std_17)
target_link_libraries(bacnet_windows_tests PRIVATE bacnet_windows)

add_executable(bacnet_cli_support_tests test/cli_support_tests.cpp)
target_compile_features(bacnet_cli_support_tests PRIVATE cxx_std_17)
target_link_libraries(bacnet_cli_support_tests PRIVATE bacnet_cli_common)

add_library(bacnet_client_write_enabled STATIC
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetClient.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetLogger.cpp)
target_compile_features(bacnet_client_write_enabled PUBLIC cxx_std_17)
target_compile_definitions(bacnet_client_write_enabled PRIVATE ESP_BACNET_ENABLE_WRITE_PROPERTY=1)
target_include_directories(bacnet_client_write_enabled PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_link_libraries(bacnet_client_write_enabled PUBLIC bacnet_portable)

add_library(bacnet_client_priority_enabled STATIC
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetClient.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetLogger.cpp)
target_compile_features(bacnet_client_priority_enabled PUBLIC cxx_std_17)
target_compile_definitions(bacnet_client_priority_enabled PRIVATE
  ESP_BACNET_ENABLE_WRITE_PROPERTY=1
  ESP_BACNET_ENABLE_PRIORITY_WRITE=1)
target_include_directories(bacnet_client_priority_enabled PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_link_libraries(bacnet_client_priority_enabled PUBLIC bacnet_portable)

add_executable(bacnet_write_property_tests test/write_property_tests.cpp)
target_compile_features(bacnet_write_property_tests PRIVATE cxx_std_17)
target_compile_definitions(bacnet_write_property_tests PRIVATE ESP_BACNET_ENABLE_WRITE_PROPERTY=1)
target_link_libraries(bacnet_write_property_tests PRIVATE bacnet_client_write_enabled)

add_executable(bacnet_write_property_disabled_tests
  test/write_property_disabled_tests.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetDeviceSession.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetRemoteObject.cpp)
target_compile_features(bacnet_write_property_disabled_tests PRIVATE cxx_std_17)
target_link_libraries(bacnet_write_property_disabled_tests PRIVATE bacnet_client)

add_executable(bacnet_priority_write_disabled_tests
  test/priority_write_disabled_tests.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetDeviceSession.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetRemoteObject.cpp)
target_compile_features(bacnet_priority_write_disabled_tests PRIVATE cxx_std_17)
target_compile_definitions(bacnet_priority_write_disabled_tests PRIVATE
  ESP_BACNET_ENABLE_WRITE_PROPERTY=1
  ESP_BACNET_ENABLE_PRIORITY_WRITE=0)
target_link_libraries(bacnet_priority_write_disabled_tests PRIVATE bacnet_client_write_enabled)

add_executable(bacnet_priority_helpers_tests
  test/priority_helpers_tests.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetClient.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetLogger.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetDeviceSession.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetRemoteObject.cpp)
target_compile_features(bacnet_priority_helpers_tests PRIVATE cxx_std_17)
target_compile_definitions(bacnet_priority_helpers_tests PRIVATE
  ESP_BACNET_ENABLE_WRITE_PROPERTY=1
  ESP_BACNET_ENABLE_PRIORITY_WRITE=1)
target_include_directories(bacnet_priority_helpers_tests PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_link_libraries(bacnet_priority_helpers_tests PRIVATE bacnet_portable)

add_executable(bacnet_write_hil_tests
  test/write_hil_tests.cpp
  cli/BacnetWriteHil.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetClient.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetLogger.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetDeviceSession.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetRemoteObject.cpp)
target_compile_features(bacnet_write_hil_tests PRIVATE cxx_std_17)
target_compile_definitions(bacnet_write_hil_tests PRIVATE
  ESP_BACNET_ENABLE_WRITE_PROPERTY=1
  ESP_BACNET_ENABLE_PRIORITY_WRITE=1)
target_include_directories(bacnet_write_hil_tests PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../../src
  ${CMAKE_CURRENT_LIST_DIR}/cli)
target_link_libraries(bacnet_write_hil_tests PRIVATE bacnet_portable)

if(BUILD_TESTING)
  add_test(NAME bacnet_windows_tests COMMAND bacnet_windows_tests)
  add_test(NAME bacnet_cli_support_tests COMMAND bacnet_cli_support_tests)
  add_test(NAME bacnet_write_property_tests COMMAND bacnet_write_property_tests)
  add_test(NAME bacnet_write_property_disabled_tests COMMAND bacnet_write_property_disabled_tests)
  add_test(NAME bacnet_priority_write_disabled_tests COMMAND bacnet_priority_write_disabled_tests)
  add_test(NAME bacnet_priority_helpers_tests COMMAND bacnet_priority_helpers_tests)
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
