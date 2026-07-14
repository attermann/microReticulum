# Example applications

Sample programs demonstrating how to use the microReticulum library. Each
subdirectory is a self-contained example with its own `src/main.cpp` and
`platformio.ini`.

Native examples (buildable via CMake on Linux, macOS, Raspberry Pi). The
CMake target name matches the directory name:

| Directory / CMake target               | What it does                                                |
|----------------------------------------|-------------------------------------------------------------|
| `udp_transport`                        | UDP-based transport node                                    |
| `udp_announce`                         | UDP announce example                                        |
| `link_native`                          | Link establishment over a native interface                  |
| `nomadnet`                             | NomadNet-style application example                          |

The CMake build also produces a shared helper library `udp_interface`
(in `examples/common/udp_interface/`) that's linked by both the UDP
examples and the interop senders in `test_interop/`.

LoRa examples (PlatformIO only — require board-specific hardware drivers):

| Directory                              | What it does                                                |
|----------------------------------------|-------------------------------------------------------------|
| `lora_transport/`                      | LoRa radio transport node                                   |
| `lora_announce/`                       | LoRa announce example                                       |
| `lora_transport_internalfs_override/`  | LoRa transport with InternalFS persistence override         |

## CMake

The native examples are built by the top-level CMake project. Configure
and build from the repository root:

Configure:
```
cmake -S . -B build
```

Build all native examples:
```
cmake --build build -j --target udp_transport udp_announce link_native nomadnet
```

Build a single example:
```
cmake --build build -j --target udp_transport
```

Run an example:
```
./build/examples/udp_transport
```

Skip building examples entirely when configuring the parent project:
```
cmake -S . -B build -DRNS_BUILD_EXAMPLES=OFF
```

The LoRa examples are intentionally excluded from the CMake build — use
PlatformIO to build those.

## PlatformIO CLI

Each example directory has its own `platformio.ini`. From inside the
example directory, build a single environment (board):
```
pio run -e native17
pio run -e ttgo-t-beam
```

Build and upload to a connected board:
```
pio run -e ttgo-t-beam -t upload
```

### Heltec Wireless Tracker V2

The Tracker V2 environment uses the board's onboard SX1262 and KCT8103L RF
front end. Attach a suitable LoRa antenna before transmitting. The example's
radio frequency is currently set to 915 MHz in
`common/lora_interface/LoRaInterface.h`; change it there if your hardware and
local band plan require another frequency.

From the repository root, build and upload the transport example with:

```
cd examples/lora_transport
pio run -e heltec-wireless-tracker-v2
pio run -e heltec-wireless-tracker-v2 -t upload
pio device monitor -b 115200
```
