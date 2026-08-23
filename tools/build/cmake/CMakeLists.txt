cmake_minimum_required(VERSION 3.16)
project(esp_bacnet_native LANGUAGES CXX)

include(CTest)

set(ESP_BACNET_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")

add_library(bacnet_portable STATIC
  ${ESP_BACNET_ROOT}/src/core/protocol/BacnetProtocol.cpp)
target_compile_features(bacnet_portable PUBLIC cxx_std_17)
target_include_directories(bacnet_portable PUBLIC
  ${ESP_BACNET_ROOT}/src)

add_library(bacnet_client STATIC
  ${ESP_BACNET_ROOT}/src/core/client/BacnetClient.cpp
  ${ESP_BACNET_ROOT}/src/support/BacnetLogger.cpp)
target_compile_features(bacnet_client PUBLIC cxx_std_17)
target_include_directories(bacnet_client PUBLIC
  ${ESP_BACNET_ROOT}/src)
target_link_libraries(bacnet_client PUBLIC bacnet_portable)

add_library(bacnet_server STATIC
  ${ESP_BACNET_ROOT}/src/core/server/BacnetServer.cpp)
target_compile_features(bacnet_server PUBLIC cxx_std_17)
target_include_directories(bacnet_server PUBLIC
  ${ESP_BACNET_ROOT}/src)
target_link_libraries(bacnet_server PUBLIC bacnet_portable)

add_executable(portable_smoke ${ESP_BACNET_ROOT}/tests/portable/portable_smoke.cpp)
target_compile_features(portable_smoke PRIVATE cxx_std_17)
target_link_libraries(portable_smoke PRIVATE bacnet_portable bacnet_server)

add_executable(server_analog_value_tests
  ${ESP_BACNET_ROOT}/tests/portable/server_analog_value_tests.cpp)
target_compile_features(server_analog_value_tests PRIVATE cxx_std_17)
target_link_libraries(server_analog_value_tests PRIVATE bacnet_portable bacnet_server)

add_executable(server_object_api_tests
  ${ESP_BACNET_ROOT}/tests/portable/server_object_api_tests.cpp)
target_compile_features(server_object_api_tests PRIVATE cxx_std_17)
target_link_libraries(server_object_api_tests PRIVATE bacnet_portable bacnet_server)

add_executable(server_device_metadata_tests
  ${ESP_BACNET_ROOT}/tests/portable/server_device_metadata_tests.cpp)
target_compile_features(server_device_metadata_tests PRIVATE cxx_std_17)
target_link_libraries(server_device_metadata_tests PRIVATE bacnet_portable bacnet_server)

add_executable(server_cov_tests ${ESP_BACNET_ROOT}/tests/portable/server_cov_tests.cpp)
target_compile_features(server_cov_tests PRIVATE cxx_std_17)
target_link_libraries(server_cov_tests PRIVATE bacnet_portable bacnet_server)

add_executable(cov_session_tests
  ${ESP_BACNET_ROOT}/tests/portable/cov_session_tests.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetDeviceSession.cpp
  ${ESP_BACNET_ROOT}/src/core/client/BacnetRemoteObject.cpp)
target_compile_features(cov_session_tests PRIVATE cxx_std_17)
target_link_libraries(cov_session_tests PRIVATE bacnet_portable bacnet_client)

add_executable(io_example_input_tests
  ${ESP_BACNET_ROOT}/tests/portable/examples/io-server/io_example_input_tests.cpp)
target_compile_features(io_example_input_tests PRIVATE cxx_std_17)
target_include_directories(io_example_input_tests PRIVATE
  ${ESP_BACNET_ROOT}/examples/esp32/shared/io-server/src)

if(BUILD_TESTING)
  add_test(NAME portable_smoke COMMAND portable_smoke)
  add_test(NAME server_analog_value_tests COMMAND server_analog_value_tests)
  add_test(NAME server_object_api_tests COMMAND server_object_api_tests)
  add_test(NAME server_device_metadata_tests COMMAND server_device_metadata_tests)
  add_test(NAME server_cov_tests COMMAND server_cov_tests)
  add_test(NAME cov_session_tests COMMAND cov_session_tests)
  add_test(NAME io_example_input_tests COMMAND io_example_input_tests)
endif()

if(WIN32)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/windows native)
endif()
