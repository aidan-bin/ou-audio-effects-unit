cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME nucleo-ou-audio-effects)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Generate WAV lookup table header from demo audio file
set(WAV_LUT_INPUT  "${CMAKE_CURRENT_SOURCE_DIR}/../../../host/python-demo/MoodySlide2_110.wav")
set(WAV_LUT_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/../../../scripts/wav_to_lut.py")
set(WAV_LUT_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/../app/include/test_vector_wav.h")

add_custom_command(
    OUTPUT ${WAV_LUT_OUTPUT}
    COMMAND python3 ${WAV_LUT_SCRIPT}
        --input ${WAV_LUT_INPUT}
        --output ${WAV_LUT_OUTPUT}
        --rate 8000
        --duration 5
        --name wav_lut
    DEPENDS ${WAV_LUT_SCRIPT} ${WAV_LUT_INPUT}
    COMMENT "Generating WAV LUT: ${WAV_LUT_OUTPUT}"
)

add_custom_target(generate_wav_lut DEPENDS ${WAV_LUT_OUTPUT})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ../app/src/log.c
    ../app/src/fault_handler.c
    ../app/src/cli_core.c
    ../app/src/cli_parse.c
    ../app/src/cli_session.c
    ../app/src/cli_service_adapter.c
    ../app/src/effects_model.c
    ../app/src/effects_pipeline.c
    ../app/src/effects_task.c
    ../app/src/test_vector_source.c
    ../app/src/usb_audio_stream.c
    ../app/src/usbd_cdc_dual.c
    ../app/src/sysinfo.c
    ../app/src/i2c_handler.c
    ../app/src/i2c_task_support.c
    ../app/src/peripheral_dispatch.c
    ../app/src/rom_handler.c
    ../app/src/rom_task_support.c
    ../../../dsp/effects/effect_runtime.c
    ../../../dsp/effects/effects.c
    ../../../dsp/math/bam.c
    ../../../dsp/math/fast_math.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ../app/include
    ../../../dsp/effects
    ../../../dsp/math
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    INCLUDE_WAV_LUT
    SYSINFO_BOARD="nucleo-f303re"
    SYSINFO_MCU="stm32f303re"
    SYSINFO_VERSION="0.1.0"
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

add_dependencies(${CMAKE_PROJECT_NAME} generate_wav_lut)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
