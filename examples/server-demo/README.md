# BACnet Server Demo

This minimal server-role placeholder preserves its existing `usb` build and
adds an `eth` environment for the WT32-ETH01 V1.4. The Ethernet build brings up
the LAN8720 interface and prints its wired IP before starting the existing
server role. The server role itself is still a placeholder and does not yet
implement a complete BACnet server runtime.

```sh
pio run -d examples/server-demo -e usb
pio run -d examples/server-demo -e eth
```

Copy `src/secret/secrets.example.h` to `src/secret/secrets.h` to override local
Ethernet settings. The `eth-com6` environment is a local test convenience only;
use the actual serial port on other systems.
