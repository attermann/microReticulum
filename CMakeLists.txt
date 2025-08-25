cmake_minimum_required(VERSION 3.14)
project(Reticulum LANGUAGES CXX)

# Set C++ standard (can be overridden via command line)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Option to fetch external libraries
option(FETCH_EXTERNAL_LIBS "Download and include external dependencies" ON)

include(FetchContent)

# Fetch external libraries
if(FETCH_EXTERNAL_LIBS)
    message(STATUS "Fetching external dependencies...")

    # ArduinoJson
    FetchContent_Declare(
        arduinojson
        GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
        GIT_TAG v7.4.2
    )
    FetchContent_MakeAvailable(arduinojson)

    # MsgPack (non-CMake library)
    FetchContent_Declare(
        msgpack
        GIT_REPOSITORY https://github.com/hideakitai/MsgPack.git
        GIT_TAG v0.4.2
    )
    FetchContent_MakeAvailable(msgpack)

    # Crypto (non-CMake library)
    FetchContent_Declare(
        crypto
        GIT_REPOSITORY https://github.com/attermann/Crypto.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(crypto)
endif()

# Include MsgPack (non-CMake library) library header files
set(MSGPACK_INCLUDE_DIR ${msgpack_SOURCE_DIR})

# Include Crypto (non-CMake library) library header files
set(CRYPTO_INCLUDE_DIR ${crypto_SOURCE_DIR})

# Include Crypto (non-CMake library) library source files
file(GLOB CRYPTO_SOURCES ${crypto_SOURCE_DIR}/*.cpp)
add_library(CryptoLib STATIC ${CRYPTO_SOURCES})
target_include_directories(CryptoLib PUBLIC ${CRYPTO_INCLUDE_DIR})

# Include directories
include_directories(src include)

# Gather source files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

# Create shared library
add_library(ReticulumShared SHARED ${SOURCES})
set_target_properties(ReticulumShared PROPERTIES OUTPUT_NAME "Reticulum")

# Create static library
add_library(ReticulumStatic STATIC ${SOURCES})
set_target_properties(ReticulumStatic PROPERTIES OUTPUT_NAME "Reticulum")

# Common compiler flags and defines
foreach(target ReticulumShared ReticulumStatic)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wno-missing-field-initializers
        -Wno-format
        -Wno-unused-parameter
    )

    target_compile_definitions(${target} PRIVATE
        LIBRARY_TEST
        RNS_USE_FS
        RNS_PERSIST_PATHS
        NATIVE
    )

    target_include_directories(${target} PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/include
    )

    # Include MsgPack (non-CMake library) headers
    target_include_directories(${target} PUBLIC
        ${MSGPACK_INCLUDE_DIR}
    )

    # Include Crypto (non-CMake library) headers
    target_include_directories(${target} PUBLIC
        ${CRYPTO_INCLUDE_DIR}
    )

    # Link external libraries if available
    if(FETCH_EXTERNAL_LIBS)
        target_link_libraries(${target} PRIVATE
            ArduinoJson
            msgpackc-cxx
            crypto
            CryptoLib
            MsgPack
        )
    endif()
endforeach()

# Optional test executable
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
    add_executable(ReticulumTest main.cpp)
    target_link_libraries(ReticulumTest PRIVATE ReticulumStatic)
endif()

