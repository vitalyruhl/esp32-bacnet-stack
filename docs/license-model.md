# License model

This project is licensed under `GPL-2.0-or-later WITH GCC-exception-2.0`.

The project is intended to provide an ESP32-friendly BACnet/IP client and
server library for Arduino and PlatformIO projects.

Future protocol implementation work may be based on, port, or reference the
open-source `bacnet-stack` project. No upstream `bacnet-stack` source files are
currently imported.

The GCC exception is important for embedded and industrial use cases: it allows applications to link against this BACnet stack without forcing the complete application firmware to become GPL-licensed. At the same time, changes made to the BACnet stack itself must remain available under the original license terms when distributed.

This keeps improvements to the BACnet stack open while still allowing the library to be used in proprietary, commercial, and industrial ESP32 firmware projects.

## Source file headers

New source files should use the following SPDX identifier:

```text
SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0
```

Imported upstream files from `bacnet-stack` must keep their original copyright
notices and SPDX license identifiers.
