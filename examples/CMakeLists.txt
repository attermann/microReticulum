# Native example applications and the shared udp_interface used by both
# examples/ and test_interop/.

# udp_interface — shared by udp_transport, udp_announce, nomadnet, link_native,
# and all four test_interop senders.
add_library(udp_interface STATIC
    common/udp_interface/UDPInterface.cpp
)
target_include_directories(udp_interface PUBLIC common/udp_interface)
target_link_libraries(udp_interface PUBLIC microReticulum)

if(NOT RNS_BUILD_EXAMPLES)
    return()
endif()

# Helper: declare a native example executable.
function(rns_add_example name src_path)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src_path}")
        message(WARNING "Skipping example '${name}': ${src_path} not found")
        return()
    endif()
    add_executable(${name} ${src_path})
    target_link_libraries(${name} PRIVATE microReticulum udp_interface)
endfunction()

rns_add_example(udp_transport udp_transport/src/main.cpp)
rns_add_example(udp_announce  udp_announce/src/main.cpp)
rns_add_example(link_native   link_native/src/main.cpp)
rns_add_example(nomadnet      nomadnet/src/main.cpp)

# LoRa examples (lora_transport, lora_announce, lora_transport_internalfs_override)
# are intentionally skipped: they depend on board-only LoRa radio drivers and
# RadioLib/Arduino. Use PlatformIO to build those.
