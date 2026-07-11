# BACnet Client Demo - WiFi

This is the preserved WiFi variant of the BACnet/IP client demo. It retains the
existing ConfigManager-driven WiFi setup, discovery, object-list scan, process
value display, and watched Analog Value behavior.

Copy `src/secret/secrets.example.h` to `src/secret/secrets.h` for local network
and BACnet target settings. The local file is ignored by Git.

Build from the repository root:

```sh
pio run -d examples/client-demo-wifi -e usb
```

For new wired BACnet deployments, see `examples/client-demo-ETH`.
