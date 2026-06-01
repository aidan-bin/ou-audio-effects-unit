cmake_minimum_required(VERSION 3.20)

project(ou_audio_effects_unit VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(OU_BUILD_SHARED_LIB "Build shared effects library for host demo" ON)
option(OU_BUILD_TESTS "Build DSP unit tests" ON)

set(OU_DSP_SOURCES
    dsp/effects/effect_runtime.c
    dsp/effects/effects.c
    dsp/math/bam.c
    dsp/math/fast_math.c
)

set(OU_FIRMWARE_APP_SOURCES
    firmware/stm32f303/app/src/button_task.c
    firmware/stm32f303/app/src/effects_state_manager.c
    firmware/stm32f303/app/src/effects_control_logic.c
    firmware/stm32f303/app/src/effects_pipeline.c
    firmware/stm32f303/app/src/effects_task.c
    firmware/stm32f303/app/src/i2c_handler.c
    firmware/stm32f303/app/src/peripheral_dispatch.c
    firmware/stm32f303/app/src/pot_task.c
    firmware/stm32f303/app/src/rom_task_support.c
    firmware/stm32f303/app/src/rom_handler.c
    firmware/stm32f303/app/src/switch_task.c
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

    if(APPLE OR UNIX)
        target_link_libraries(ou_effects_shared PRIVATE m)
    endif()

    set_target_properties(ou_effects_shared PROPERTIES
        OUTPUT_NAME libeffects
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/host/python-demo
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/host/python-demo
    )

    if(WIN32)
        set_target_properties(ou_effects_shared PROPERTIES SUFFIX ".dll")
    endif()
endif()

add_library(ou_firmware_app STATIC ${OU_FIRMWARE_APP_SOURCES})
target_include_directories(ou_firmware_app
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/firmware/stm32f303/app/include
        ${CMAKE_CURRENT_SOURCE_DIR}/dsp/effects
)

target_link_libraries(ou_firmware_app PRIVATE ou_dsp)

if(OU_BUILD_TESTS)
    enable_testing()

    add_library(ou_firmware_mocks STATIC
        tests/mocks/hal_mock.c
        tests/mocks/freertos_mock.c
    )

    target_include_directories(ou_firmware_mocks
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )

    add_library(ou_firmware_host_harness STATIC
        tests/harness/host_harness.c
        tests/harness/host_ops.c
    )

    target_include_directories(ou_firmware_host_harness
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
            ${CMAKE_CURRENT_SOURCE_DIR}/firmware/stm32f303/app/include
    )

    target_link_libraries(ou_firmware_host_harness
        PUBLIC
            ou_firmware_app
            ou_firmware_mocks
    )

    add_executable(ou_dsp_tests
        tests/test_dsp.c
    )

    target_link_libraries(ou_dsp_tests PRIVATE ou_dsp)

    if(APPLE OR UNIX)
        target_link_libraries(ou_dsp_tests PRIVATE m)
    endif()

    add_test(NAME ou_dsp_tests COMMAND ou_dsp_tests)

    add_executable(ou_firmware_state_tests
        tests/test_firmware_state.c
    )

    target_include_directories(ou_firmware_state_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/firmware/stm32f303/app/include
    )

    target_link_libraries(ou_firmware_state_tests PRIVATE ou_firmware_app)

    if(APPLE OR UNIX)
        target_link_libraries(ou_firmware_state_tests PRIVATE m)
    endif()

    add_test(NAME ou_firmware_state_tests COMMAND ou_firmware_state_tests)

    add_executable(ou_firmware_control_logic_tests
        tests/test_firmware_control_logic.c
    )

    target_include_directories(ou_firmware_control_logic_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/firmware/stm32f303/app/include
    )

    target_link_libraries(ou_firmware_control_logic_tests PRIVATE ou_firmware_app)

    if(APPLE OR UNIX)
        target_link_libraries(ou_firmware_control_logic_tests PRIVATE m)
    endif()

    add_test(NAME ou_firmware_control_logic_tests COMMAND ou_firmware_control_logic_tests)

    add_executable(ou_firmware_effects_pipeline_tests
        tests/test_firmware_effects_pipeline.c
    )

    target_include_directories(ou_firmware_effects_pipeline_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/firmware/stm32f303/app/include
            ${CMAKE_CURRENT_SOURCE_DIR}/dsp/effects
    )

    target_link_libraries(ou_firmware_effects_pipeline_tests PRIVATE ou_firmware_app)

    if(APPLE OR UNIX)
        target_link_libraries(ou_firmware_effects_pipeline_tests PRIVATE m)
    endif()

    add_test(NAME ou_firmware_effects_pipeline_tests COMMAND ou_firmware_effects_pipeline_tests)

    add_executable(ou_firmware_effects_task_tests
        tests/test_firmware_effects_task.c
    )

    target_include_directories(ou_firmware_effects_task_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/firmware/stm32f303/app/include
            ${CMAKE_CURRENT_SOURCE_DIR}/dsp/effects
    )

    target_link_libraries(ou_firmware_effects_task_tests PRIVATE ou_firmware_app)

    if(APPLE OR UNIX)
        target_link_libraries(ou_firmware_effects_task_tests PRIVATE m)
    endif()

    add_test(NAME ou_firmware_effects_task_tests COMMAND ou_firmware_effects_task_tests)

    add_executable(ou_firmware_pot_task_tests
        tests/test_firmware_pot_task.c
    )

    target_include_directories(ou_firmware_pot_task_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/firmware/stm32f303/app/include
            ${CMAKE_CURRENT_SOURCE_DIR}/dsp/effects
    )

    target_link_libraries(ou_firmware_pot_task_tests PRIVATE ou_firmware_app)

    if(APPLE OR UNIX)
        target_link_libraries(ou_firmware_pot_task_tests PRIVATE m)
    endif()

    add_test(NAME ou_firmware_pot_task_tests COMMAND ou_firmware_pot_task_tests)

    add_executable(ou_firmware_switch_task_tests
        tests/test_firmware_switch_task.c
    )

    target_include_directories(ou_firmware_switch_task_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/firmware/stm32f303/app/include
            ${CMAKE_CURRENT_SOURCE_DIR}/dsp/effects
    )

    target_link_libraries(ou_firmware_switch_task_tests PRIVATE ou_firmware_app)

    if(APPLE OR UNIX)
        target_link_libraries(ou_firmware_switch_task_tests PRIVATE m)
    endif()

    add_test(NAME ou_firmware_switch_task_tests COMMAND ou_firmware_switch_task_tests)

    add_executable(ou_firmware_button_task_tests
        tests/test_firmware_button_task.c
    )

    target_include_directories(ou_firmware_button_task_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/firmware/stm32f303/app/include
    )

    target_link_libraries(ou_firmware_button_task_tests PRIVATE ou_firmware_app)

    if(APPLE OR UNIX)
        target_link_libraries(ou_firmware_button_task_tests PRIVATE m)
    endif()

    add_test(NAME ou_firmware_button_task_tests COMMAND ou_firmware_button_task_tests)

    add_executable(ou_firmware_task_integration_tests
        tests/test_firmware_task_integration.c
    )

    target_include_directories(ou_firmware_task_integration_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/firmware/stm32f303/app/include
            ${CMAKE_CURRENT_SOURCE_DIR}/dsp/effects
    )

    target_link_libraries(ou_firmware_task_integration_tests PRIVATE ou_firmware_app)

    if(APPLE OR UNIX)
        target_link_libraries(ou_firmware_task_integration_tests PRIVATE m)
    endif()

    add_test(NAME ou_firmware_task_integration_tests COMMAND ou_firmware_task_integration_tests)

    add_executable(ou_firmware_rom_task_support_tests
        tests/test_firmware_rom_task_support.c
    )

    target_include_directories(ou_firmware_rom_task_support_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/firmware/stm32f303/app/include
    )

    target_link_libraries(ou_firmware_rom_task_support_tests PRIVATE ou_firmware_app)

    if(APPLE OR UNIX)
        target_link_libraries(ou_firmware_rom_task_support_tests PRIVATE m)
    endif()

    add_test(NAME ou_firmware_rom_task_support_tests COMMAND ou_firmware_rom_task_support_tests)

    add_executable(ou_firmware_peripheral_mocks_tests
        tests/test_firmware_peripheral_mocks.c
    )

    target_link_libraries(ou_firmware_peripheral_mocks_tests PRIVATE ou_firmware_mocks)

    if(APPLE OR UNIX)
        target_link_libraries(ou_firmware_peripheral_mocks_tests PRIVATE m)
    endif()

    add_test(NAME ou_firmware_peripheral_mocks_tests COMMAND ou_firmware_peripheral_mocks_tests)

    add_executable(ou_firmware_peripheral_dispatch_tests
        tests/test_firmware_peripheral_dispatch.c
    )

    target_include_directories(ou_firmware_peripheral_dispatch_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/firmware/stm32f303/app/include
    )

    target_link_libraries(ou_firmware_peripheral_dispatch_tests PRIVATE ou_firmware_app)

    if(APPLE OR UNIX)
        target_link_libraries(ou_firmware_peripheral_dispatch_tests PRIVATE m)
    endif()

    add_test(NAME ou_firmware_peripheral_dispatch_tests COMMAND ou_firmware_peripheral_dispatch_tests)

    add_executable(ou_firmware_i2c_handler_tests
        tests/test_firmware_i2c_handler.c
    )

    target_include_directories(ou_firmware_i2c_handler_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/firmware/stm32f303/app/include
    )

    target_link_libraries(ou_firmware_i2c_handler_tests PRIVATE ou_firmware_app ou_firmware_mocks)

    if(APPLE OR UNIX)
        target_link_libraries(ou_firmware_i2c_handler_tests PRIVATE m)
    endif()

    add_test(NAME ou_firmware_i2c_handler_tests COMMAND ou_firmware_i2c_handler_tests)

    add_executable(ou_firmware_rom_handler_tests
        tests/test_firmware_rom_handler.c
    )

    target_include_directories(ou_firmware_rom_handler_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/firmware/stm32f303/app/include
    )

    target_link_libraries(ou_firmware_rom_handler_tests PRIVATE ou_firmware_app)

    if(APPLE OR UNIX)
        target_link_libraries(ou_firmware_rom_handler_tests PRIVATE m)
    endif()

    add_test(NAME ou_firmware_rom_handler_tests COMMAND ou_firmware_rom_handler_tests)
endif()
