cmake_minimum_required(VERSION 3.16)
project(esp_bacnet_native LANGUAGES CXX)

include(CTest)

add_library(bacnet_portable STATIC
  ${CMAKE_CURRENT_LIST_DIR}/../../src/portable/BacnetProtocol.cpp)
target_compile_features(bacnet_portable PUBLIC cxx_std_17)
target_include_directories(bacnet_portable PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../../src)

add_library(bacnet_client STATIC
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetClient.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetLogger.cpp)
target_compile_features(bacnet_client PUBLIC cxx_std_17)
target_include_directories(bacnet_client PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_link_libraries(bacnet_client PUBLIC bacnet_portable)

add_library(bacnet_server STATIC
  ${CMAKE_CURRENT_LIST_DIR}/../../src/BacnetServer.cpp)
target_compile_features(bacnet_server PUBLIC cxx_std_17)
target_include_directories(bacnet_server PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_link_libraries(bacnet_server PUBLIC bacnet_portable)

add_executable(portable_smoke portable_smoke.cpp)
target_compile_features(portable_smoke PRIVATE cxx_std_17)
target_link_libraries(portable_smoke PRIVATE bacnet_portable bacnet_server)

add_executable(server_analog_value_tests server_analog_value_tests.cpp)
target_compile_features(server_analog_value_tests PRIVATE cxx_std_17)
target_link_libraries(server_analog_value_tests PRIVATE bacnet_portable bacnet_server)

if(BUILD_TESTING)
  add_test(NAME portable_smoke COMMAND portable_smoke)
  add_test(NAME server_analog_value_tests COMMAND server_analog_value_tests)
endif()

if(WIN32)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../native native)
endif()
