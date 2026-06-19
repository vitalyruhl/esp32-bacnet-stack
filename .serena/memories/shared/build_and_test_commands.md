# Build And Test Commands

- Status: commands below are verified from repository configuration,
  documentation, or canonical governance.
- Required default validation after C++ source/header changes:
  `pio run -e usb`
- Compile tests without upload:
  `pio test -e usb --without-uploading --without-testing`
- Build the client demo:
  `pio run -d examples/client-demo -e usb`
- Build the server demo:
  `pio run -d examples/server-demo -e usb`
- Clean the selected PlatformIO project:
  `pio run -t clean`
- Upload, monitor, erase, OTA, and hardware/network-affecting commands are not
  validation defaults and require explicit user approval.
- Enabled GitHub Actions workflow: `.github/workflows/pio-tests.yml`.

Sources: `.github/AGENTS.md`, `platformio.ini`, `README.md`,
`.github/workflows/pio-tests.yml`, `test/README`.
