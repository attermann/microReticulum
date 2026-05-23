# Unity-based test suites — mirrors `pio test`.

include(FetchContent)

if(RNS_UNITY_SOURCE_DIR AND IS_DIRECTORY "${RNS_UNITY_SOURCE_DIR}")
    message(STATUS "Using local Unity from ${RNS_UNITY_SOURCE_DIR}")
    FetchContent_Declare(unity SOURCE_DIR "${RNS_UNITY_SOURCE_DIR}")
else()
    FetchContent_Declare(unity
        GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
        GIT_TAG        v2.6.0
    )
endif()
FetchContent_MakeAvailable(unity)

# Discover every test/test_* directory and build each as its own executable.
file(GLOB RNS_TEST_DIRS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*")

foreach(test_dir IN LISTS RNS_TEST_DIRS)
    set(abs_dir "${CMAKE_CURRENT_SOURCE_DIR}/${test_dir}")
    if(NOT IS_DIRECTORY "${abs_dir}")
        continue()
    endif()

    file(GLOB suite_sources CONFIGURE_DEPENDS "${abs_dir}/*.cpp")
    if(NOT suite_sources)
        continue()
    endif()

    add_executable(${test_dir} ${suite_sources})
    target_link_libraries(${test_dir} PRIVATE microReticulum unity)
    target_include_directories(${test_dir} PRIVATE "${abs_dir}")
    target_compile_definitions(${test_dir} PRIVATE PIO_UNIT_TESTING)

    add_test(NAME ${test_dir} COMMAND ${test_dir})
endforeach()
