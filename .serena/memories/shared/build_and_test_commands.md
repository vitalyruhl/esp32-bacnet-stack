# Build And Test Commands

- Status: commands below are verified from repository configuration,
  documentation, or canonical governance.
- Required default validation after C++ source/header changes:
  `pio run -e usb`
- Compile tests without upload:
  `pio test -e usb --without-uploading --without-testing`
- Build the serial-only object-list client example:
  `pio run -d examples/client-object-list-scan-basic -e usb`
- Build the WiFi client demo:
  `pio run -d examples/client-demo-wifi -e usb`
- Build the Ethernet client demo:
  `pio run -d examples/client-demo-ETH -e eth`
- Build the server demo for both configured transports:
  `pio run -d examples/server-demo -e usb -e eth`
- Build the local HIL WAGO acceptance example only when explicitly in scope:
  `pio run -d examples/hil-wago-client-acceptance -e usb`
- Build its WT32-ETH01 Ethernet environment when wired HIL is in scope:
  `pio run -d examples/hil-wago-client-acceptance -e eth`
- Clean the selected PlatformIO project:
  `pio run -t clean`
- Upload, monitor, erase, OTA, and hardware/network-affecting commands are not
  validation defaults and require explicit user approval.
- Enabled GitHub Actions workflow: `.github/workflows/pio-tests.yml`.
- Dependabot is configured for GitHub Actions updates only in `.github/dependabot.yml`.
- The HIL WAGO acceptance example is local-hardware-only and must not be treated as a default validation requirement.

Sources: `.github/AGENTS.md`, `platformio.ini`, `README.md`,
`.github/workflows/pio-tests.yml`, `.github/dependabot.yml`, `test/README`,
`examples/**/platformio.ini`, `examples/hil-wago-client-acceptance/README.md`.
