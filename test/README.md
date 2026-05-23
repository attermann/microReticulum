# Unit tests

Unity-based unit test suites covering the microReticulum library. Each
`test_*/` subdirectory builds into its own executable and is registered
as a CTest test of the same name.

## Targets

```
test_allocator                 test_msgpack             test_resource_advertisement
test_bytes                     test_objects             test_rns_loopback
test_collections               test_os                  test_rns_persistence
test_crypto                    test_persistence         test_transport
test_example                   test_reference
test_filesystem
test_general
```

## CMake

These tests are built by the top-level CMake project. Configure and build
from the repository root:

Configure (defaults: C++17, Release):
```
cmake -S . -B build
```

Build all test binaries:
```
cmake --build build -j
```

Build a single test suite:
```
cmake --build build -j --target test_msgpack
```

Run all tests:
```
ctest --test-dir build --output-on-failure
```

Run a single test (exact name):
```
ctest --test-dir build -R '^test_msgpack$' --output-on-failure
```

Run a single test binary directly (useful for attaching a debugger):
```
./build/test/test_msgpack
```

Skip building tests entirely when configuring the parent project:
```
cmake -S . -B build -DRNS_BUILD_TESTS=OFF
```

## PlatformIO CLI

Run all tests (default environment):
```
pio test
```

Run a single test suite:
```
pio test -f test_msgpack
```

Run a single test suite on a specific environment:
```
pio test -f test_msgpack -e native17
```
