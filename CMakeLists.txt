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
    firmware/stm32f303/app/src/effects_state_manager.c
    firmware/stm32f303/app/src/effects_control_logic.c
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
endif()
