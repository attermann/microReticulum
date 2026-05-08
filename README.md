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

## Memory Management Build Options

Two classes of memory allocator are defined; the container allocator (`RNS_CONTAINER_ALLOCATOR`) which is used for certain long-lived STL containers (e.g. path table), and the default allocator (`RNS_DEFAULT_ALLOCATOR`) which is used for all other C++ memory allocation (new/delete).

Each class of allocator can be configured to use a separate type of allocator. Available memory allocators are:
- `RNS_HEAP_ALLOCATOR`: Basic allocator that uses default system HEAP.
- `RNS_HEAP_POOL_ALLOCATOR`: Optimized TLSF managed HEAP buffer
- `RNS_PSRAM_ALLOCATOR`: Basic allocator that uses PSRAM memory (if PSRAM is available)
- `RNS_PSRAM_POOL_ALLOCATOR`: Optimized TLSF managed PSRAM buffer (if PSRAM is available)

NOTE: When utilizing PSRAM on ESP32 boards, preprocessor directive `-DBOARD_HAS_PSRAM=1` must be included in build flags in order to enable on-board PSRAM.

Each class of allocator pool can have a separate buffer size:
- `RNS_HEAP_POOL_BUFFER_SIZE=N` Defines the HEAP TLSF memory pool buffer size (N bytes, default 0 which means dynamic)
- `RNS_PSRAM_POOL_BUFFER_SIZE=N` Defines the PSRAM TLSF memory pool buffer size (N bytes, default 0 which means dynamic)

"POOL" allocators use TLSF (Two-Level Segregate Fit) for efficient management of constrained MCU memory with minimal fragmentation.

The allocator for each class can be configured using build flags in `platformio.ini, for example:

```
[env]
build_flags = 
  -DRNS_DEFAULT_ALLOCATOR=RNS_HEAP_ALLOCATOR
  -DRNS_CONTAINER_ALLOCATOR=RNS_PSRAM_POOL_ALLOCATOR
```

NOTE: Currently "POOL" allocators are only required on NRF52 boards since ESP32 already uses TLSF internally for both SRAM and PSRAM resources.

## microStore Options

When using microReticulum to implement a transpport node, storage for the path table and other persistent data must be configured. This is accomplished using the File and FileSystem abstrctions provided in `microStore`. Several pre-build adapters are provided with the library, and optionally custom storage (e.g., external flash and SD card) can be used by deriving custom implementations from `microStore::File` and `microStore::FileSystem` and passing them to microReticulum for internal use.
There are several pre-built filesystem "adapters" available in the `microStore` library that can be used without modification for the most common supported boards. These can be inluded using the following preprocessor directives:
  `-DUSTORE_USE_POSIXFS`: Native as well as any ESP-IDF supported POSIX file system
  `-DUSTORE_USE_STDIOFS`: Native STDIO file system
  `-DUSTORE_USE_INTERNALFS`: InternalFileSystem for adafruit nrf52
  `-DUSTORE_USE_LITTLEFS`: LittleFS file system
  `-DUSTORE_USE_SPIFFS`: SPIFFS file system
  `-DUSTORE_USE_FLASHFS`: Adafruit FlashFS file system
  `-DUSTORE_USE_UNIVERSALFS`: Best supported file system for any platform

Also note the following preprocessor directives used to tune `microStore::FileStore` which is the Bitcask-style storage engine used by microReticulum to persist the path table and other internal tables:
  `-DRNS_PATH_TABLE_SEGMENT_SIZE`: Size in bytes of each segment file
  `-DRNS_PATH_TABLE_SEGMENT_COUNT`: Maximum number of segment files to rotate (minimum of 3)
Appropriate settings should be selected to match the storage and memory resources available on the target platform.

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

## Known Limitations

Nordic nrf52840 based boards are severely constrained, especially in avialable flash storage available to the application. Even though the nrf52840 has 1 MB of flash, the `InternalFileSystem` implemented inside of the Adafruit nrf52 BSP hard-codes the flash filesystem size to only 28 KB, which severely limits the amount of data theat microReticulum can persist (especially the path table).
Use of nrf52840 based boards as a transport node will necessitate the use of external storage for persistence.

## Known Issues


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
- [x] Implement new combination caching/storage manager for managing persistent data

Please open an Issue if you have trouble building ior using the API, and feel free to start a new Discussion for anything else.

