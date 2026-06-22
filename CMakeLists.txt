cmake_minimum_required(VERSION 3.15)
project(microReticulum LANGUAGES C CXX VERSION 0.1.0)

# -----------------------------------------------------------------------------
# Defaults
# -----------------------------------------------------------------------------
if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------
option(RNS_BUILD_TESTS    "Build Unity test suites under test/"          ON)
option(RNS_BUILD_EXAMPLES "Build native example applications"            ON)
option(RNS_BUILD_INTEROP  "Build interop senders under test_interop/"    ON)
option(RNS_USE_FS           "Enable filesystem persistence"                                ON)
option(RNS_PERSIST_PATHS    "Persist path table to storage"                                ON)
option(RNS_USE_PROVISIONING "Auto-start Provisioning subsystem from Reticulum::start()"    ON)
option(RNS_DEBUG_MEMORY   "Enable memory/heap/metrics debug logging"     OFF)
option(RNS_SANITIZE       "Build with AddressSanitizer + frame pointers" OFF)

set(RNS_DEFAULT_ALLOCATOR      "" CACHE STRING "Override RNS_DEFAULT_ALLOCATOR (e.g. RNS_HEAP_POOL_ALLOCATOR)")
set(RNS_CONTAINER_ALLOCATOR    "" CACHE STRING "Override RNS_CONTAINER_ALLOCATOR")
set(RNS_HEAP_POOL_BUFFER_SIZE  "" CACHE STRING "TLSF heap pool size (bytes)")

# Local-checkout overrides for each fetched dependency. Mirrors PIO's symlink://
# option: point a CMake var at a local clone and skip the network fetch.
set(RNS_ARDUINOJSON_SOURCE_DIR    "" CACHE PATH "Use local ArduinoJson checkout instead of FetchContent")
set(RNS_MSGPACK_SOURCE_DIR        "" CACHE PATH "Use local MsgPack checkout instead of FetchContent")
set(RNS_CRYPTO_SOURCE_DIR         "" CACHE PATH "Use local Crypto checkout instead of FetchContent")
set(RNS_HEATSHRINK_SOURCE_DIR     "" CACHE PATH "Use local Heatshrink checkout instead of FetchContent")
set(RNS_MICROSTORE_SOURCE_DIR     "" CACHE PATH "Use local microStore checkout instead of FetchContent")
set(RNS_ARXCONTAINER_SOURCE_DIR   "" CACHE PATH "Use local ArxContainer checkout instead of FetchContent")
set(RNS_ARXTYPETRAITS_SOURCE_DIR  "" CACHE PATH "Use local ArxTypeTraits checkout instead of FetchContent")
set(RNS_DEBUGLOG_SOURCE_DIR       "" CACHE PATH "Use local DebugLog checkout instead of FetchContent")
set(RNS_UNITY_SOURCE_DIR          "" CACHE PATH "Use local Unity checkout instead of FetchContent")

# -----------------------------------------------------------------------------
# Dependency fetching
# -----------------------------------------------------------------------------
include(FetchContent)

# rns_fetch(<name> <override_path_var> <git url> <git tag>)
# Either uses the local override path (if set) or fetches from upstream.
# Defined as a macro (not a function) so the <name>_SOURCE_DIR variable that
# FetchContent_MakeAvailable sets is visible in the parent scope.
macro(rns_fetch name override_var url tag)
    if(${override_var} AND IS_DIRECTORY "${${override_var}}")
        message(STATUS "Using local ${name} from ${${override_var}}")
        FetchContent_Declare(${name} SOURCE_DIR "${${override_var}}")
    else()
        FetchContent_Declare(${name} GIT_REPOSITORY "${url}" GIT_TAG "${tag}")
    endif()
    FetchContent_MakeAvailable(${name})
endmacro()

rns_fetch(arduinojson    RNS_ARDUINOJSON_SOURCE_DIR    https://github.com/bblanchon/ArduinoJson.git v7.4.2)
rns_fetch(msgpack        RNS_MSGPACK_SOURCE_DIR        https://github.com/hideakitai/MsgPack.git v0.4.2)
rns_fetch(arxcontainer   RNS_ARXCONTAINER_SOURCE_DIR   https://github.com/hideakitai/ArxContainer.git v0.7.0)
rns_fetch(arxtypetraits  RNS_ARXTYPETRAITS_SOURCE_DIR  https://github.com/hideakitai/ArxTypeTraits.git v0.3.2)
rns_fetch(debuglog       RNS_DEBUGLOG_SOURCE_DIR       https://github.com/hideakitai/DebugLog.git v0.8.4)
rns_fetch(heatshrink     RNS_HEATSHRINK_SOURCE_DIR     https://github.com/atomicobject/heatshrink.git v0.4.1)
# Crypto and microStore have no upstream tags suitable for pinning, so pin commit SHAs.
# Refresh by running `git ls-remote <url> HEAD` and updating the hex below.
rns_fetch(crypto         RNS_CRYPTO_SOURCE_DIR         https://github.com/attermann/Crypto.git)
rns_fetch(microstore     RNS_MICROSTORE_SOURCE_DIR     https://github.com/attermann/microStore.git)

# -----------------------------------------------------------------------------
# Third-party targets that don't ship CMakeLists.txt
# -----------------------------------------------------------------------------
# Crypto: flat directory of .cpp files at the root.
file(GLOB CRYPTO_SOURCES CONFIGURE_DEPENDS "${crypto_SOURCE_DIR}/*.cpp")
add_library(CryptoLib STATIC ${CRYPTO_SOURCES})
target_include_directories(CryptoLib PUBLIC "${crypto_SOURCE_DIR}")
set_target_properties(CryptoLib PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Header-only / Arduino-style libraries — expose their include roots.
add_library(MsgPackHeaders       INTERFACE)
target_include_directories(MsgPackHeaders       INTERFACE "${msgpack_SOURCE_DIR}")
add_library(ArxContainerHeaders  INTERFACE)
target_include_directories(ArxContainerHeaders  INTERFACE "${arxcontainer_SOURCE_DIR}")
add_library(ArxTypeTraitsHeaders INTERFACE)
target_include_directories(ArxTypeTraitsHeaders INTERFACE "${arxtypetraits_SOURCE_DIR}")
add_library(DebugLogHeaders      INTERFACE)
target_include_directories(DebugLogHeaders      INTERFACE "${debuglog_SOURCE_DIR}")
add_library(HeatshrinkHeaders    INTERFACE)
target_include_directories(HeatshrinkHeaders    INTERFACE "${heatshrink_SOURCE_DIR}")
add_library(microStoreHeaders    INTERFACE)
target_include_directories(microStoreHeaders    INTERFACE "${microstore_SOURCE_DIR}/include")

# -----------------------------------------------------------------------------
# microReticulum library
# -----------------------------------------------------------------------------
# src/main.cpp self-gates on `LIBRARY_TEST && !PIO_UNIT_TESTING`. As long as we
# don't define LIBRARY_TEST here, it compiles to an empty TU. Glob is safe.
file(GLOB_RECURSE RNS_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.c)

add_library(microReticulum ${RNS_SOURCES})
set_target_properties(microReticulum PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(microReticulum PUBLIC
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/include"
)

target_compile_options(microReticulum PRIVATE
    -Wall
    -Wno-missing-field-initializers
    -Wno-format
    -Wno-unused-parameter
)

# PUBLIC compile definitions — translation units that consume our public
# headers (tests, examples, interop senders) must see the same macros or
# things like the OS.h filesystem wrappers and persistence types ODR-diverge.
target_compile_definitions(microReticulum PUBLIC
    NATIVE
    USTORE_USE_UNIVERSALFS
)
if(RNS_USE_FS)
    target_compile_definitions(microReticulum PUBLIC RNS_USE_FS)
endif()
if(RNS_PERSIST_PATHS)
    target_compile_definitions(microReticulum PUBLIC RNS_PERSIST_PATHS)
endif()
if(RNS_USE_PROVISIONING)
    target_compile_definitions(microReticulum PUBLIC RNS_USE_PROVISIONING)
endif()
if(RNS_DEBUG_MEMORY)
    target_compile_definitions(microReticulum PUBLIC
        RNS_DEBUG_HEAP RNS_DEBUG_MEMORY RNS_DEBUG_METRICS RNS_DEBUG_PATHSTORE)
endif()
if(RNS_DEFAULT_ALLOCATOR)
    target_compile_definitions(microReticulum PUBLIC "RNS_DEFAULT_ALLOCATOR=${RNS_DEFAULT_ALLOCATOR}")
endif()
if(RNS_CONTAINER_ALLOCATOR)
    target_compile_definitions(microReticulum PUBLIC "RNS_CONTAINER_ALLOCATOR=${RNS_CONTAINER_ALLOCATOR}")
endif()
if(RNS_HEAP_POOL_BUFFER_SIZE)
    target_compile_definitions(microReticulum PUBLIC "RNS_HEAP_POOL_BUFFER_SIZE=${RNS_HEAP_POOL_BUFFER_SIZE}")
endif()

if(RNS_SANITIZE)
    target_compile_options(microReticulum PUBLIC -fsanitize=address -fno-omit-frame-pointer -g)
    target_link_options(microReticulum    PUBLIC -fsanitize=address)
endif()

target_link_libraries(microReticulum
    PUBLIC
        ArduinoJson
        MsgPackHeaders
        ArxContainerHeaders
        ArxTypeTraitsHeaders
        DebugLogHeaders
        HeatshrinkHeaders
        microStoreHeaders
        CryptoLib
)

# -----------------------------------------------------------------------------
# Tests, examples, interop senders
# -----------------------------------------------------------------------------
if(RNS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

if(RNS_BUILD_EXAMPLES OR RNS_BUILD_INTEROP)
    # examples/ defines the shared udp_interface target used by both examples
    # and the interop senders, so it must be added first.
    add_subdirectory(examples)
endif()

if(RNS_BUILD_INTEROP)
    add_subdirectory(test_interop)
endif()
