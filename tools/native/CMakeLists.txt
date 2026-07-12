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
  ${CMAKE_CURRENT_LIST_DIR}/cli/BacnetCliSupport.cpp)
target_compile_features(bacnet_cli_common PUBLIC cxx_std_17)
target_include_directories(bacnet_cli_common PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../../src
  ${CMAKE_CURRENT_LIST_DIR}/cli)

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

if(BUILD_TESTING)
  add_test(NAME bacnet_windows_tests COMMAND bacnet_windows_tests)
  add_test(NAME bacnet_cli_support_tests COMMAND bacnet_cli_support_tests)
  add_test(NAME bacnet_cli_help COMMAND bacnet_cli_smoke --help)
  add_test(NAME bacnet_cli_self_test COMMAND bacnet_cli_smoke --self-test)
  add_test(NAME bacnet_cli_unknown_option COMMAND bacnet_cli_smoke --unknown)
  set_tests_properties(bacnet_cli_unknown_option PROPERTIES WILL_FAIL TRUE)
  add_test(NAME bacnet_discover_help COMMAND bacnet-discover-smoke --help)
  add_test(NAME bacnet_discover_self_test COMMAND bacnet-discover-smoke --self-test)
  add_test(NAME bacnet_discover_invalid_arguments COMMAND bacnet-discover-smoke --bind 127.0.0.1)
  set_tests_properties(bacnet_discover_invalid_arguments PROPERTIES WILL_FAIL TRUE)
endif()
