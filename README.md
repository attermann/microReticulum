# microReticulum

Port of Reticulum Network Stack to C++ specifically but not exclusively targeting 32-bit and better MCUs.

## Dependencies

Build environment is configured for use in [VSCode](https://code.visualstudio.com/) and [PlatformIO](https://platformio.org/).

This API is dependent on the following external libraries:
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
- [rweather/Crypto](https://github.com/rweather/arduinolibs)

## Building

Building and uploading to hardware is simple through the VSCode PlatformIO IDE
- Install VSCode and PlatformIO
- Clone this repo
- Lanch PlatformIO and load repo
- In PlatformIO, select the environment for intended board
- Build, Upload, and Monitor to observe application logging

Instructions for command line builds and building of individual example applications coming soon.

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

