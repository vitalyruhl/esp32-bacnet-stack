# BACnet Client Demo - WiFi

This is the WiFi variant of the BACnet/IP client demo. Apart from the network
adapter and connection settings, it uses the same ConfigManager-driven BACnet
monitoring and bounded Property Browser as the Ethernet variant. The default
build exposes the configured Binary Value as a read-only Boolean indicator
with `active`, `inactive`, or `unknown` state. The status uses controlled
polling and never sends a write.

Copy `src/secret/secrets.example.h` to `src/secret/secrets.h` for local network
and BACnet target settings. The local file is ignored by Git.

Build from the repository root:

```sh
pio run -d examples/client-demo-wifi -e usb
```

The default `usb` build exposes no active WriteProperty controls. For an
explicit local HIL write check only, use the separately gated environment:

```sh
pio run -d examples/client-demo-wifi -e usb_write
```

`usb_write` enables the existing Manual Priority Overrides card; every action
still sends at most one explicit request and an active priority must be
relinquished explicitly.

For new wired BACnet deployments, see `examples/client-demo-ETH`.
