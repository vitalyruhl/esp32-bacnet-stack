# BACnet Client Demo - WiFi

This is the WiFi variant of the BACnet/IP client demo. Apart from the network
adapter and connection settings, it uses the same ConfigManager-driven BACnet
monitoring and manual-override interface as the Ethernet variant. The Manual
Priority Overrides card shows the configured Binary Value as a read-only
Boolean indicator with `active`, `inactive`, or `unknown` state. The status
uses controlled polling and never sends a write. Write buttons remain guarded
by the build-time WriteProperty and Priority features.

Copy `src/secret/secrets.example.h` to `src/secret/secrets.h` for local network
and BACnet target settings. The local file is ignored by Git.

Build from the repository root:

```sh
pio run -d examples/client-demo-wifi -e usb
```

For new wired BACnet deployments, see `examples/client-demo-ETH`.
