cmake_minimum_required(VERSION 3.20)

project(ou_audio_effects_unit VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(OU_BUILD_SHARED_LIB "Build shared effects library for host demo" ON)
option(OU_BUILD_TESTS "Build DSP unit tests" ON)

set(OU_FIRMWARE_CUBEMX_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/stm32f303/cubemx
)
set(OU_FIRMWARE_BUILD_TYPE "Debug" CACHE STRING "Build type used for the CubeMX firmware build target")
set_property(CACHE OU_FIRMWARE_BUILD_TYPE PROPERTY STRINGS Debug Release)

set(OU_FIRMWARE_APP_INCLUDE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/stm32f303/app/include
)

set(OU_FIRMWARE_APP_STANDARD_TESTS
    effects_model
    effects_pipeline
    effects_task
    cli_core
    cli_line_buffer
    cli_session
    cli_service_adapter
    pot_task
    switch_task
    button_task
    task_integration
    rom_task_support
    peripheral_dispatch
    rom_handler
)

function(ou_link_math_library target_name)
    if(APPLE OR UNIX)
        target_link_libraries(${target_name} PRIVATE m)
    endif()
endfunction()

function(ou_add_firmware_app_test test_basename)
    set(test_target ou_firmware_${test_basename}_tests)

    add_executable(${test_target}
        tests/test_firmware_${test_basename}.c
    )

    target_include_directories(${test_target}
        PRIVATE
            ${OU_FIRMWARE_APP_INCLUDE_DIR}
    )

    target_link_libraries(${test_target} PRIVATE ou_firmware_app)
    ou_link_math_library(${test_target})

    add_test(NAME ${test_target} COMMAND ${test_target})
endfunction()

set(OU_DSP_SOURCES
    dsp/effects/effect_runtime.c
    dsp/effects/effects.c
    dsp/math/bam.c
    dsp/math/fast_math.c
)

set(OU_FIRMWARE_APP_SOURCES
    firmware/stm32f303/app/src/button_task.c
    firmware/stm32f303/app/src/cli_core.c
    firmware/stm32f303/app/src/cli_line_buffer.c
    firmware/stm32f303/app/src/cli_parse.c
    firmware/stm32f303/app/src/cli_session.c
    firmware/stm32f303/app/src/cli_service_adapter.c
    firmware/stm32f303/app/src/effects_model.c
    firmware/stm32f303/app/src/effects_pipeline.c
    firmware/stm32f303/app/src/effects_task.c
    firmware/stm32f303/app/src/i2c_handler.c
    firmware/stm32f303/app/src/peripheral_dispatch.c
    firmware/stm32f303/app/src/pot_task.c
    firmware/stm32f303/app/src/rom_task_support.c
    firmware/stm32f303/app/src/rom_handler.c
    firmware/stm32f303/app/src/switch_task.c
    firmware/stm32f303/app/src/test_vector_source.c
)

add_library(ou_dsp STATIC ${OU_DSP_SOURCES})
target_include_directories(ou_dsp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/dsp/effects
        ${CMAKE_CURRENT_SOURCE_DIR}/dsp/math
)

if(OU_BUILD_SHARED_LIB)
    add_library(ou_effects_shared SHARED ${OU_DSP_SOURCES})
    target_include_directories(ou_effects_shared
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/dsp/effects
            ${CMAKE_CURRENT_SOURCE_DIR}/dsp/math
    )

    ou_link_math_library(ou_effects_shared)

    set_target_properties(ou_effects_shared PROPERTIES
        OUTPUT_NAME libeffects
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/host/python-demo
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/host/python-demo
    )

    if(WIN32)
        set_target_properties(ou_effects_shared PROPERTIES SUFFIX ".dll")
    endif()

    add_custom_target(ou_build_demo DEPENDS ou_effects_shared)
endif()

add_library(ou_firmware_app STATIC ${OU_FIRMWARE_APP_SOURCES})
target_include_directories(ou_firmware_app
    PUBLIC
    ${OU_FIRMWARE_APP_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/dsp/effects
)

target_link_libraries(ou_firmware_app PRIVATE ou_dsp)

if(EXISTS ${OU_FIRMWARE_CUBEMX_DIR}/CMakeLists.txt)
    set(OU_FIRMWARE_CUBEMX_BUILD_DIR ${OU_FIRMWARE_CUBEMX_DIR}/build/${OU_FIRMWARE_BUILD_TYPE})

    add_custom_target(ou_firmware_build
        COMMAND ${CMAKE_COMMAND}
            -S ${OU_FIRMWARE_CUBEMX_DIR}
            -B ${OU_FIRMWARE_CUBEMX_BUILD_DIR}
            -DCMAKE_BUILD_TYPE=${OU_FIRMWARE_BUILD_TYPE}
            -DCMAKE_TOOLCHAIN_FILE=${OU_FIRMWARE_CUBEMX_DIR}/cmake/gcc-arm-none-eabi.cmake
        COMMAND ${CMAKE_COMMAND} --build ${OU_FIRMWARE_CUBEMX_BUILD_DIR}
        COMMENT "Configure and build CubeMX firmware (${OU_FIRMWARE_BUILD_TYPE})"
        USES_TERMINAL
    )
endif()

if(OU_BUILD_TESTS)
    enable_testing()

    set(OU_TEST_BUILD_TARGETS)

    add_library(ou_firmware_mocks STATIC
        tests/mocks/hal_mock.c
        tests/mocks/freertos_mock.c
    )

    target_include_directories(ou_firmware_mocks
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )

    add_executable(ou_dsp_tests
        tests/test_dsp.c
    )

    target_link_libraries(ou_dsp_tests PRIVATE ou_dsp)
    ou_link_math_library(ou_dsp_tests)
    add_test(NAME ou_dsp_tests COMMAND ou_dsp_tests)
    list(APPEND OU_TEST_BUILD_TARGETS ou_dsp_tests)

    foreach(test_basename IN LISTS OU_FIRMWARE_APP_STANDARD_TESTS)
        ou_add_firmware_app_test(${test_basename})
        list(APPEND OU_TEST_BUILD_TARGETS ou_firmware_${test_basename}_tests)
    endforeach()

    add_executable(ou_firmware_peripheral_mocks_tests
        tests/test_firmware_peripheral_mocks.c
    )

    target_link_libraries(ou_firmware_peripheral_mocks_tests PRIVATE ou_firmware_mocks)
    ou_link_math_library(ou_firmware_peripheral_mocks_tests)
    add_test(NAME ou_firmware_peripheral_mocks_tests COMMAND ou_firmware_peripheral_mocks_tests)
    list(APPEND OU_TEST_BUILD_TARGETS ou_firmware_peripheral_mocks_tests)

    add_executable(ou_firmware_i2c_handler_tests
        tests/test_firmware_i2c_handler.c
    )

    target_include_directories(ou_firmware_i2c_handler_tests
        PRIVATE
            ${OU_FIRMWARE_APP_INCLUDE_DIR}
    )

    target_link_libraries(ou_firmware_i2c_handler_tests PRIVATE ou_firmware_app ou_firmware_mocks)
    ou_link_math_library(ou_firmware_i2c_handler_tests)
    add_test(NAME ou_firmware_i2c_handler_tests COMMAND ou_firmware_i2c_handler_tests)
    list(APPEND OU_TEST_BUILD_TARGETS ou_firmware_i2c_handler_tests)

    add_custom_target(ou_build_tests DEPENDS ${OU_TEST_BUILD_TARGETS})
endif()
