# Notices and Disclaimers

## Copyright and license

Original Codex Micro compatibility implementation:

Copyright (c) 2026 imliubo.

M5StickC port and subsequent modifications:

Copyright (c) 2026 PolarisLight.

Source code and documentation are licensed under the [MIT License](LICENSE).
The MIT License does not grant rights to third-party trademarks, product names,
hardware designs, protocols, or other intellectual property.

## Upstream attribution

This project is derived from
[`imliubo/codex-micro-4-core2`](https://github.com/imliubo/codex-micro-4-core2).
The upstream project supplied the original BLE HID descriptor, framing, and
Codex Micro compatibility implementation. This repository adds an original
M5StickC interface, IMU-driven layouts, local title synchronization, power
behavior, recovery logic, and hybrid battery estimation.

## Independent project and trademarks

This is an independent, unofficial compatibility project. It is not affiliated
with, authorized by, endorsed by, sponsored by, or supported by OpenAI, Work
Louder, M5Stack, or any of their affiliates.

OpenAI, ChatGPT, and Codex are trademarks or registered trademarks of OpenAI.
Work Louder, Codex Micro, M5Stack, and M5StickC names and marks belong to their
respective owners. They are used only to identify compatibility.

## Protocol and compatibility disclaimer

The firmware implements behavior observed from an undocumented vendor HID
interface. Device names, manufacturer strings, identifiers, usage pages, report
IDs, key IDs, and RPC names are used solely for compatibility. Compatibility
may change or stop working after host software updates.

## Hardware, security, and data disclaimer

Flashing third-party firmware can erase existing software or settings and may
leave hardware temporarily unusable. BLE pairing uses a “Just Works” flow.
Operate only in a trusted environment and keep a recovery image.

The firmware does not connect directly to OpenAI. The optional title-sync helper
reads local Codex assignment metadata and sends the titles to the StickC over
USB serial. Review the source before use.

## No warranty

THE SOFTWARE AND DOCUMENTATION ARE PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY
KIND. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE AUTHORS AND COPYRIGHT HOLDERS
ARE NOT LIABLE FOR ANY CLAIM, DAMAGE, LOSS, OR OTHER LIABILITY ARISING FROM USE,
MODIFICATION, FLASHING, PAIRING, DISTRIBUTION, OR INABILITY TO USE THIS PROJECT.
