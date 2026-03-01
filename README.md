# microReticulum

Port of Reticulum Network Stack to C++ specifically but not exclusively targeting 32-bit and better MCUs.

Note that this is a library implementing the Reticulum Network Stack and not a useful application on its own. For an actual application, see [microReticulum_Firmware](https://github.com/attermann/microReticulum_Firmware) which is a version of RNode firmware that implements a Reticulm Transport Node.

## Dependencies

Build environment is configured for use in [VSCode](https://code.visualstudio.com/) and [PlatformIO](https://platformio.org/).

This API is dependent on the following external libraries:
- [rweather/Crypto](https://github.com/rweather/arduinolibs)
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
- [MsgPack](https://github.com/hideakitai/MsgPack)

## Build Options

- `-DRNS_MEM_LOG` Used to enable logging of low-level memory operations for debug purposes
- `-DRNS_USE_FS` Used to enable use of file system by RNS for persistence
- `-DRNS_PERSIST_PATHS` Used to enable persistence of RNS paths in file system (also requires `-DRNS_USE_FS`)
- `-DRNS_USE_TLSF=1` Enables the use of the TLSF (Two-Level Segregate Fit) dynamic memory manager for efficient management of constrained MCU memory with minimal fragmentation. Currently only required on NRF52 boards (ESP32 already uses TLSF internally).
- `-RNS_TLSF_BUFFER_SIZ=N` Defines the TLSF memory pool buffer size (N bytes, default 0 which means dynamic) 
- `-DRNS_USE_ALLOCATOR=1` Enables the replacement of default new/delete operators with custom implementations that take advantage of optimized memory managers (eg, TLSF). Currently only required on NRF52 boards (ESP32 already uses TLSF internally).

## Building

Building and uploading to hardware is simple through the VSCode PlatformIO IDE
- Install VSCode and PlatformIO
- Clone this repo
- Lanch PlatformIO and load repo
- In PlatformIO, select the environment for intended board
- Build, Upload, and Monitor to observe application logging

Instructions for command line builds and building of individual example applications coming soon.

## PlatformIO Command Line

Clean all environments (boards):
```
pio run -t clean
```

Full Clean (including libdeps) all environments (boards):
```
pio run -t fullclean
```

Run unit tests:
```
pio test
pio test -f test_msgpack
pio test -f test_msgpack -e native
pio test -f test_msgpack -e native17
```

Build a single environment (board):
```
pio run -e ttgo-t-beam
pio run -e wiscore_rak4631
```

Build and upload a single environment (board):
```
pio run -e ttgo-t-beam -t upload
pio run -e wiscore_rak4631 -t upload
```

Build and package a single environment (board):
```
pio run -e ttgo-t-beam -t package
pio run -e wiscore_rak4631 -t package
```

Build all environments (boards):
```
pio run
```

## Known Issues

Use of the custom allocator (enabled by `-DRNS_USE_ALLOCATOR=1`) in conjunction with TLSF (enabled by `-DRNS_USE_TLSF=1`) is not currently working in ESP32 due to TLFS initialization failure at the time that allocator initialization takes place. This causes the allocator to fallback to using regular malloc/free instead of using TLFS as intended.

## Roadmap

- [x] Framework for simple C++ API that mimics Python reference implementation with implicit object sharing
- [x] Utility class to efficiently handle byte buffers
- [x] Implemenmtat basic Identity, Destination and Packet
- [x] Successful testing of Announce mechanism
- [x] Implement Ed2551 public-key signature and X25519 key exchange
- [x] Implement encryption including AES, AES-CBC, HKDF, HMAC, PKCS7, and Fernet
- [x] Successful testing of Packet encrypt/decrypt/sign/prof
- [x] Implement dynamic Interfaces
- [x] Implement UDP Interface for testing against Python reference Reticulum instances
- [x] Implement basic Transport functionality
- [x] Successful testing of end-to-end Path Finding
- [x] Implement persistence for storage of runtime data structures (config, identities, routing tables, etc.)
- [x] Support for Links and Resources in Transport
- [x] Implement Packet proofs
- [x] Successful testing of Transport of both Packets and Links
- [x] Implement memory management appropriate for memory constricted MCUs
- [x] Implement Link
- [x] Implement AES256
- [ ] Implement Ratchets
- [ ] Implement Resource
- [ ] Implement Channel and Buffer
- [x] Add example applications utilizing the C++ API
- [x] Configure PlatformIO to easily build and upload test applications to board
- [x] Add build tagets for all boards currently supported by RNode
- [ ] Implement new combination caching/storage manager for managing persistent data

Please open an Issue if you have trouble building ior using the API, and feel free to start a new Discussion for anything else.

