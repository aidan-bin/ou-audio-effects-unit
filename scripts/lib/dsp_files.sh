#!/usr/bin/env bash

DSP_FORMAT_FILES=(
    dsp/effects/effects.c
    dsp/effects/effects.h
    dsp/math/fast_math.c
    dsp/math/fast_math.h
)

DSP_LINT_FILES=(
    dsp/effects/effects.c
    dsp/math/fast_math.c
)

APP_HEADER_FILES=(
    $(git -C "$REPO_ROOT" ls-files 'firmware/stm32f303/app/include/*.h')
)

APP_SOURCE_FILES=(
    $(git -C "$REPO_ROOT" ls-files 'firmware/stm32f303/app/src/*.c')
)

APP_FORMAT_FILES=(
    "${APP_HEADER_FILES[@]}"
    "${APP_SOURCE_FILES[@]}"
)

APP_LINT_FILES=(
    "${APP_SOURCE_FILES[@]}"
)

CUBEMX_USER_CODE_FILES=(
    firmware/stm32f303/cubemx/Core/Src/main.c
    firmware/stm32f303/cubemx/Core/Src/freertos.c
    firmware/stm32f303/cubemx/Core/Src/stm32f3xx_hal_msp.c
    firmware/stm32f303/cubemx/Core/Src/stm32f3xx_hal_timebase_tim.c
    firmware/stm32f303/cubemx/Core/Src/stm32f3xx_it.c
    firmware/stm32f303/cubemx/Core/Src/syscalls.c
    firmware/stm32f303/cubemx/Core/Src/sysmem.c
    firmware/stm32f303/cubemx/Core/Src/system_stm32f3xx.c
)