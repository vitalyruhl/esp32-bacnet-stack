# WAGO BACnet/IP Test Server

This folder contains the exported WAGO BACnet/IP test server configuration used
for ESP32 client-demo validation.

## Device

- Controller: WAGO `750-8212/0000-0100`
- Firmware: `2.3.1 / 04.09.01(31)`
- BACnet device object: `BACNet_Testserver`
- BACnet device instance: `3`
- BACnet/IP UDP port: `47808`

The `BACNet_Server_GPO/` CSV files are the object-manager exports. The
`wago-project/` files and `BACNet_Testserver_2026.06.30.wbc` capture the WAGO
project state used to program the server.

## Programmed Test Data

The server exports test objects for client discovery and ReadProperty checks:

| Object range | Purpose |
| --- | --- |
| `BACnet_Analog_Value_200` to `BACnet_Analog_Value_205` | Analog Value samples with zero, mid-range, fixed, changing, and out-of-service cases |
| `BACnet_Binary_Value_100` to `BACnet_Binary_Value_102` | Binary Value samples for inactive and commanded inactive reads |
| `TTT_MULTISTATE_VALUE_2000` to `TTT_MULTISTATE_VALUE_2009` | Multi-State Value samples using the state text `1-OK`, `STATE_2`, `STATE_3`, `STATE_4`, `5-Warning`, and `6-Error` |

The CSV descriptions are intentionally human-readable because the client demo
prefers BACnet `description` over `objectName` when rendering discovered AV and
MV objects.
