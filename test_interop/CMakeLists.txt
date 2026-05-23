# Interop senders — C++ side of the C++ <-> Python interop test scripts in
# this directory. Each suite gets its own UDP port pair so they can coexist.
# Port values mirror each sender's platformio.ini.

# rns_add_interop(<target> <src_path> <local_port> <remote_port>)
function(rns_add_interop name src_path local_port remote_port)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src_path}")
        message(WARNING "Skipping interop sender '${name}': ${src_path} not found")
        return()
    endif()
    add_executable(${name} ${src_path})
    target_link_libraries(${name} PRIVATE microReticulum udp_interface)
    target_compile_definitions(${name} PRIVATE
        "DEFAULT_UDP_LOCAL_HOST=\"127.0.0.1\""
        "DEFAULT_UDP_REMOTE_HOST=\"127.0.0.1\""
        "DEFAULT_UDP_LOCAL_PORT=${local_port}"
        "DEFAULT_UDP_REMOTE_PORT=${remote_port}"
        RNS_DEBUG_HEAP
        RNS_DEBUG_MEMORY
        RNS_DEBUG_METRICS
    )
endfunction()

rns_add_interop(packet_interop_sender   packet_interop_sender/src/main.cpp   14253 14252)
rns_add_interop(link_interop_sender     link_interop_sender/src/main.cpp     14255 14254)
rns_add_interop(request_interop_sender  request_interop_sender/src/main.cpp  14257 14256)
rns_add_interop(resource_interop_sender resource_interop_sender/src/main.cpp 14243 14242)
