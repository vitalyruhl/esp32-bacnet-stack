# ESP32 I/O Example Development-Station Connection Reference

This is the authoritative physical connection reference for
`examples/server-io-example` on the ESP32-WROOM-32 / ESP32-D0WD-V3 development board.
It documents the real station. The current slice initializes the read-only
inputs and the two LED Binary Outputs; relays remain reserved.

| Function | GPIO | Current use / constraint |
| --- | ---: | --- |
| LDR divider wiper | 36 (VP) | ADC1, input-only; 3.3 V -> LDR -> pot terminal 3, wiper/terminal 2 -> GPIO36, terminal 1 -> GND |
| Divider reference | 39 (VN) | tied to GND; input-only and not available to another binding |
| DS18B20 data | 18 | 4.7 kOhm pull-up to 3.3 V |
| Reset button | 14 | button to GND, `INPUT_PULLUP`, low-active |
| Mid button | 33 | button to GND, `INPUT_PULLUP`, low-active |
| Set button | 4 | button to GND, `INPUT_PULLUP`, low-active; strapping pin |
| OLED / BME280 SDA | 21 | shared I2C bus |
| OLED / BME280 SCL | 22 | shared I2C bus |
| LED 1 / BO0 | 25 | GPIO -> resistor -> LED -> GND; active-high by default, commandable |
| LED 2 / BO1 | 26 | GPIO -> resistor -> LED -> GND; active-high by default, commandable |
| Relay 1, reserved | 19 | not driven; moved from GPIO12 to avoid its boot strapping function |
| Relay 2, reserved | 27 | not driven |
| Relay 3, reserved | 23 | not driven |

## Boot and bus notes

- GPIO4 is a strapping pin and is the confirmed `SET` input. It must not be
  forced into an unsafe level during reset or boot.
- GPIO12 is a strapping pin and is no longer connected to Relay 1. It remains
  unused by this development-station slice.
- GPIO16 was formerly noted as RS485 RX in an older station sketch. It is not
  used by the current read-only input slice.
- GPIO34-GPIO39 are input-only and have no internal pull resistors. WiFi uses
  ADC1 GPIO32-GPIO39 for reliable analog reads; ADC2 is not used for inputs.
- The OLED and BME280 remain physically connected on the I2C bus for station
  parity, but `io-example` does not initialize either device or publish BME280
  measurements.
