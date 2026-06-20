> [!IMPORTANT]
> **The directory structure in this library has changed!**
> Please note that as of version 0.4.0 the directory structure in this library has changed.
> A master header file **microReticulum.h** has been added and all other files moved to the "microReticulum" subdirectory to avoid name collision with other libraries.
> Most projects should only require including the master header file like `<microReticulum.h>`, but if individual header access is required for any reason then include like `<microReticulum/Bytes.h>`.

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
- `-DRNS_USE_PROVISIONING` Used to enable the Provisioning subsystem (auto-started from `Reticulum::start()`). Disk persistence within the subsystem is additionally gated on `-DRNS_USE_FS`. Without this flag, none of the provisioning code is linked into the final binary &mdash; see the [Provisioning](#provisioning) section below.

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

## Provisioning

microReticulum ships an optional schema-driven configuration subsystem under `src/microReticulum/Provisioning/`. It maintains a registry of namespaces and fields, holds a "working" config in RAM plus a "draft" overlay for pending edits, and (when filesystem support is enabled) persists committed values atomically to flash. Apps frame and transport MsgPack request/response payloads using their preferred link (KISS, BLE GATT, Web Serial, etc.); the library handles everything from the engine inward.

Two build flags gate the subsystem:
- `-DRNS_USE_PROVISIONING` &mdash; `Reticulum::start()` auto-calls `Provisioner::begin()`. Without the flag, nothing in `src/microReticulum/Provisioning/` is linked into the final binary; apps configure microReticulum entirely through the existing fluent setters as before.
- `-DRNS_USE_FS` &mdash; independently controls disk persistence inside the subsystem. Without it the working/draft model still works in RAM, but commits are lost on reboot.

### Registering a namespace

Apps register their own namespaces (typically in firmware bootstrap, *before* `Reticulum::start()` runs). Pick a namespace id &ge; 100 (1&ndash;99 are reserved for the library):

```cpp
#include <Provisioning/Provisioning.h>

constexpr uint16_t NS_RADIO = 100;
constexpr uint16_t FLD_FREQ = 1;
constexpr uint16_t FLD_SF   = 2;

void register_my_namespaces() {
    using namespace RNS::Provisioning;
    Provisioner::instance()
        .register_namespace("radio", NS_RADIO)
        .field_float("frequency", FLD_FREQ, FF_REBOOT_REQUIRED, 915.0e6, 100e6, 1e9,
            [](const Value& v) { my_radio.frequency(v.as_float()); return true; })
        .field_int("sf", FLD_SF, FF_LIVE_APPLY, 8, 7, 12,
            [](const Value& v) { my_radio.spreading_factor((uint8_t)v.as_int()); return true; })
        .end();
}
```

Each field declares its type, an id, flags (`FF_LIVE_APPLY` for fields that take effect immediately on commit; `FF_REBOOT_REQUIRED` for fields applied on next boot), a default, range constraints, and a setter callback that pushes the value into the app's runtime.

### Wire (MsgPack) operations

The app receives a MsgPack-encoded request payload (already stripped of whatever transport framing it uses) and feeds it to `handle_message()`:

```cpp
RNS::Bytes request  = receive_from_transport();
RNS::Bytes response = RNS::Provisioning::Provisioner::instance().handle_message(request);
send_to_transport(response);
```

Supported operations are `GET_SCHEMA`, `GET_INFO`, `GET_CAPABILITIES`, `GET_STATE`, `SET_STATE`, `COMMIT`, `DISCARD`, and `FACTORY_RESET`. The envelope is a 3-element MsgPack array `[op_id, seq, payload]`; see `src/microReticulum/Provisioning/Ops.h` for op-id and key constants.

### Manual operations (no wire)

In-process code can edit and commit without going through MsgPack. Edits land in the draft layer; only `commit()` promotes them to the working layer and (if `RNS_USE_FS` is defined) persists them:

```cpp
auto& mgr = RNS::Provisioning::Provisioner::instance();
namespace R = RNS::Provisioning::Ns::Reticulum;

// Edit draft (multiple calls accumulate)
mgr.field(R::Id, R::Field::PersistInterval, RNS::Provisioning::Value{600});
mgr.field("reticulum", "remote_management_enabled", RNS::Provisioning::Value{true});  // by-name also works

// Promote draft -> working, fire LIVE_APPLY setters, persist to flash
mgr.commit();

// Or throw the draft away
mgr.discard();
```

### Handling reboot

Commits to `FF_REBOOT_REQUIRED` fields are persisted to flash but not applied to the runtime until the next boot. The app can poll `needs_reboot()` or register a callback that fires the moment a reboot becomes necessary:

```cpp
RNS::Provisioning::Provisioner::instance().on_reboot_required([]{
    INFO("Provisioning committed a reboot-required change.");
});
```

A separate `on_reboot` callback fires only when the client sends the explicit `REBOOT` wire op &mdash; that's where most integrations actually call `ESP.restart()` / `NVIC_SystemReset()`. There is also an `on_factory_reset` hook that fires after `FACTORY_RESET` has completed its internal work, for app-side cleanup. See [docs/provisioning_integration_guide.md](docs/provisioning_integration_guide.md) for details.

The `needs_reboot()` flag stays sticky from the commit until an actual reboot. On the next boot, `Provisioner::begin()` reloads the persisted values and fires the setter callbacks for any field whose committed value differs from its declared default &mdash; so `FF_REBOOT_REQUIRED` values take effect exactly once at startup without further intervention.

## Building

### PlatformIO IDE

Building and uploading to hardware is simple through the VSCode PlatformIO IDE
- Install VSCode and PlatformIO
- Clone this repo
- Lanch PlatformIO and load repo
- In PlatformIO, select the environment for intended board
- Build, Upload, and Monitor to observe application logging

### PlatformIO CLI

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

### CMake

A standalone CMake build is provided for native targets (Linux, macOS, Raspberry Pi). It requires only CMake (≥ 3.15), a C++17-capable compiler, and `git` — all external dependencies are downloaded automatically via `FetchContent` on the first configure. ESP32 and NRF52 cross-builds remain on PlatformIO.

Configure the build (defaults: C++17, Release, all targets enabled):
```
cmake -S . -B build
```

Build everything (library, tests, examples, interop senders):
```
cmake --build build -j
```

Run unit tests:
```
ctest --test-dir build --output-on-failure
ctest --test-dir build -R test_msgpack
```

Build only the library:
```
cmake --build build -j --target microReticulum
```

Build as a shared library instead of static:
```
cmake -S . -B build -DBUILD_SHARED_LIBS=ON
```

Build with a different C++ standard:
```
cmake -S . -B build -DCMAKE_CXX_STANDARD=11
cmake -S . -B build -DCMAKE_CXX_STANDARD=20
```

Debug build with AddressSanitizer:
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DRNS_SANITIZE=ON
```

Skip building optional components:
```
cmake -S . -B build -DRNS_BUILD_TESTS=OFF -DRNS_BUILD_EXAMPLES=OFF -DRNS_BUILD_INTEROP=OFF
```

Use a local checkout of a dependency instead of fetching (mirrors PIO's `symlink://`). `<DEP>` is one of `MICROSTORE`, `CRYPTO`, `ARDUINOJSON`, `MSGPACK`, `ARXCONTAINER`, `ARXTYPETRAITS`, `DEBUGLOG`, `UNITY`:
```
cmake -S . -B build -DRNS_MICROSTORE_SOURCE_DIR=../microStore
```

Clean the build:
```
cmake --build build --target clean
```

Full clean (including fetched dependencies):
```
rm -rf build
```

#### Targets

The CMake build defines the following named targets. Any of them can be
passed to `cmake --build build --target <name>` to build that target alone
(plus its dependencies); ctest invokes the `test_*` targets.

Library:

| Target           | Kind            | What it is                                              |
|------------------|-----------------|---------------------------------------------------------|
| `microReticulum` | static / shared | The library itself; `BUILD_SHARED_LIBS` picks the kind  |
| `CryptoLib`      | static          | Compiled from the fetched `attermann/Crypto` sources    |
| `udp_interface`  | static          | Shared UDP interface used by examples and interop tests |

Unit-test suites (each is an executable; see [`test/`](test/README.md)):

```
test_allocator                 test_msgpack             test_resource_advertisement
test_bytes                     test_objects             test_rns_loopback
test_collections               test_os                  test_rns_persistence
test_crypto                    test_persistence         test_transport
test_example                   test_reference
test_filesystem
test_general
```

Native example applications (see [`examples/`](examples/README.md)):

```
udp_transport   udp_announce   link_native   nomadnet
```

Interop senders (see [`test_interop/`](test_interop/README.md)):

```
packet_interop_sender   link_interop_sender   request_interop_sender   resource_interop_sender
```

#### CMake-specific options

Knobs that only make sense in the CMake build (no PlatformIO equivalent):

| Option                                                         | Default   | Effect                                                                                            |
|----------------------------------------------------------------|-----------|---------------------------------------------------------------------------------------------------|
| `-DBUILD_SHARED_LIBS=ON`                                       | `OFF`     | Build `microReticulum` as a shared library instead of a static archive                            |
| `-DCMAKE_CXX_STANDARD=11\|14\|17\|20`                          | `17`      | Override the C++ standard                                                                         |
| `-DCMAKE_BUILD_TYPE=Debug\|Release\|RelWithDebInfo\|MinSizeRel` | `Release` | CMake build type                                                                                  |
| `-DRNS_BUILD_TESTS=OFF`                                        | `ON`      | Skip the `test/test_*` Unity suites                                                               |
| `-DRNS_BUILD_EXAMPLES=OFF`                                     | `ON`      | Skip the native examples in `examples/`                                                           |
| `-DRNS_BUILD_INTEROP=OFF`                                      | `ON`      | Skip the interop senders in `test_interop/`                                                       |
| `-DRNS_SANITIZE=ON`                                            | `OFF`     | Add `-fsanitize=address -fno-omit-frame-pointer -g`                                               |
| `-DRNS_<DEP>_SOURCE_DIR=/path`                                 | (unset)   | Use a local checkout of a dependency instead of fetching (mirrors PIO's `symlink://`). `<DEP>` is one of `MICROSTORE`, `CRYPTO`, `ARDUINOJSON`, `MSGPACK`, `ARXCONTAINER`, `ARXTYPETRAITS`, `DEBUGLOG`, `UNITY`. |

The env-agnostic preprocessor flags documented above in [Build Options](#build-options), [Memory Management Build Options](#memory-management-build-options), and [microStore Options](#microstore-options) work in the CMake build too — pass them with `-D` on the configure line, same name and value as you would in `platformio.ini`:
```
cmake -S . -B build -DRNS_USE_FS=OFF -DRNS_DEFAULT_ALLOCATOR=RNS_HEAP_POOL_ALLOCATOR
```

One naming overlap to be aware of: the CMake build defines `-DRNS_DEBUG_MEMORY=ON` as a *convenience switch* that turns on `-DRNS_DEBUG_HEAP`, `-DRNS_DEBUG_MEMORY`, `-DRNS_DEBUG_METRICS`, and `-DRNS_DEBUG_PATHSTORE` together. In PlatformIO, those four preprocessor flags are added individually in `build_flags`.

## Known Limitations

Nordic nrf52840 based boards are severely constrained, especially in avialable flash storage available to the application. Even though the nrf52840 has 1 MB of flash, the `InternalFileSystem` implemented inside of the Adafruit nrf52 BSP hard-codes the flash filesystem size to only 28 KB, which severely limits the amount of data theat microReticulum can persist (especially the path table).
Use of nrf52840 based boards as a transport node will necessitate the use of external storage for persistence, for example the RAK15001 module for RAK4631 boards which is now supported in this project.

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
- [x] Implement Resource
- [ ] Implement Channel and Buffer
- [x] Add example applications utilizing the C++ API
- [x] Configure PlatformIO to easily build and upload test applications to board
- [x] Add build tagets for all boards currently supported by RNode
- [x] Implement new combination caching/storage manager for managing persistent data
- [x] Announce example app
- [x] Transport Node example app
- [x] Link example app
- [x] NomadNet example app
- [x] Add flexible provisioning subsystem

Please open an Issue if you have trouble building ior using the API, and feel free to start a new Discussion for anything else.

