cmake_minimum_required(VERSION 3.20)

project(ou_audio_effects_unit VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(OU_BUILD_SHARED_LIB "Build shared effects library for host demo" ON)

set(OU_DSP_SOURCES
    dsp/effects/effects.c
    dsp/math/fast_math.c
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
        SUFFIX ".dll"
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/host/python-demo
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/host/python-demo
    )
endif()
