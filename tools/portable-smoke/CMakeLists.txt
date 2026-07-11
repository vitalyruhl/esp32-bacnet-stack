cmake_minimum_required(VERSION 3.16)
project(esp_bacnet_portable_smoke LANGUAGES CXX)

add_executable(portable_smoke portable_smoke.cpp)
target_compile_features(portable_smoke PRIVATE cxx_std_17)
target_include_directories(portable_smoke PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_sources(portable_smoke PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../../src/portable/BacnetProtocol.cpp)
