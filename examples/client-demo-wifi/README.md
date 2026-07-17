# BACnet Client Demo - WiFi

This is the WiFi variant of the BACnet/IP client demo. Apart from the network
adapter and connection settings, it uses the same ConfigManager-driven BACnet
monitoring, bounded Property Browser, and controlled manual overrides as the
Ethernet variant. The default build exposes the configured Binary Value as a
read-only Boolean indicator with `active`, `inactive`, or `unknown` state.
The status uses controlled polling and never sends a write.

Copy `src/secret/secrets.example.h` to `src/secret/secrets.h` for local network
and BACnet target settings. The local file is ignored by Git.

Build from the repository root:

```sh
pio run -d examples/client-demo-wifi -e usb
```

The canonical `usb` build enables the same compile-time WriteProperty and
priority-write gates as the Ethernet demo. The shared Manual Priority Overrides
card still sends at most one request for an explicit user click, and every
active priority must be relinquished explicitly.

For new wired BACnet deployments, see `examples/client-demo-ETH`.
