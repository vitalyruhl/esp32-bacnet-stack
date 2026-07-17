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

The Property Browser is incremental: its status progresses through `queued`,
`reading-property-list`, `reading-properties`, and a terminal state while the
web UI remains available. Object_List scan failures remain visible rather than
being replaced by configured monitoring objects; the configured AV/BV cards
continue to operate independently.

For new wired BACnet deployments, see `examples/client-demo-ETH`.
