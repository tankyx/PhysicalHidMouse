# PhysicalHidMouse

Write mouse input to a **real USB HID device** on Linux, from C++17.

An MCU (RP2040, ESP32-P4, …) presents itself to the PC as a mouse. This header
opens the matching `/dev/hidraw` node and writes HID input reports to it, so the
pointer moves because a USB device said so — not because software synthesised
an event.

```
host app ──write()──> /dev/hidrawN ──USB──> MCU ──USB HID──> OS pointer
```

## Why not /dev/uinput

Both move the pointer and neither touches a game process, but they are not the
same thing:

| | `/dev/uinput` | this library |
|---|---|---|
| What it is | kernel *virtual* device our process creates | *physical* USB HID on the bus |
| Game sees | a normal mouse | a normal mouse |
| OS / anti-cheat sees | a synthetic device: named, no USB topology | an actual USB mouse; nothing to fingerprint |
| Needs | `/dev/uinput` write access | `/dev/hidraw` write access |

`/dev/uinput` is indistinguishable **to the game** but distinguishable **to the
OS**. A real USB device is indistinguishable to both.

## Protocol

Output report id `2`, 64-byte payload; the report id is prepended for the
hidraw write (65 bytes total):

| byte | meaning |
|---|---|
| `[0]` | report id = `0x02` |
| `[1..2]` | `dx`, int16 little-endian |
| `[3..4]` | `dy`, int16 little-endian |
| `[5]` | button mask — bit0 Left, bit1 Right, bit2 Middle, bit3 X1, bit4 X2 |
| `[6..64]` | zero |

Matches the `RP2040_Aimer` / ESP32-P4 bridge firmware.

## Install

Header-only. Add the repo as a submodule and link the interface target:

```sh
git submodule add git@github.com:tankyx/PhysicalHidMouse.git third_party/PhysicalHidMouse
```

```cmake
add_subdirectory(third_party/PhysicalHidMouse)
target_link_libraries(your_app PRIVATE physical_hid_mouse)
```

Give your user access to the device:

```sh
sudo cp third_party/PhysicalHidMouse/70-physical-hid-mouse.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

(or add your user to the `input` group and re-login).

## Use

```cpp
#include <physical_hid_mouse.h>

phm::PhysicalHidMouse mouse;
if (!mouse.open()) return 1;          // defaults to the OP1 8K v2 identity

mouse.move(12, -3);                   // relative counts
mouse.move(0, 0, phm::Left);          // hold left button
mouse.release();
```

`open(vendor, product, serial)` takes any VID/PID; `serial` may be empty to
match on VID/PID alone. The device is reopened automatically if it goes away.

## Test

```sh
cmake -B build && cmake --build build
ctest --test-dir build --output-on-failure   # 77 = no device, skipped
./build/phm_selftest --move                  # wiggle the pointer 2 counts
```
