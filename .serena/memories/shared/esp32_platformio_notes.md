# ESP32 And PlatformIO Notes

- Status: verified.
- C++17 is required. Root and example PlatformIO configurations remove GNU++11
  and add `-std=gnu++17`.
- The root environment is `usb`, using `platform = espressif32`,
  `board = nodemcu-32s`, and `framework = arduino`.
- Current examples are `examples/client-demo` and `examples/server-demo`.
- Core library metadata currently has no ESP32 Configuration Manager dependency
  and no other library dependencies.
- Example projects consume the repository through `lib_deps = symlink://../..`
  so local build artifacts are not copied into the temporary library package.
- Secrets, local WiFi details, serial ports, OTA addresses, and credentials are
  local data; never put them in shared Serena memories.

Sources: `platformio.ini`, `examples/**/platformio.ini`, `library.json`.
