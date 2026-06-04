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
    firmware/stm32f303/app/include/button_task.h
    firmware/stm32f303/app/include/effects_model.h
    firmware/stm32f303/app/include/effects_pipeline.h
    firmware/stm32f303/app/include/effects_task.h
    firmware/stm32f303/app/include/i2c_handler.h
    firmware/stm32f303/app/include/i2c_task_support.h
    firmware/stm32f303/app/include/peripheral_dispatch.h
    firmware/stm32f303/app/include/pot_task.h
    firmware/stm32f303/app/include/rom_handler.h
    firmware/stm32f303/app/include/rom_task_support.h
    firmware/stm32f303/app/include/switch_task.h
)

APP_SOURCE_FILES=(
    firmware/stm32f303/app/src/button_task.c
    firmware/stm32f303/app/src/effects_model.c
    firmware/stm32f303/app/src/effects_pipeline.c
    firmware/stm32f303/app/src/effects_task.c
    firmware/stm32f303/app/src/i2c_handler.c
    firmware/stm32f303/app/src/i2c_task_support.c
    firmware/stm32f303/app/src/peripheral_dispatch.c
    firmware/stm32f303/app/src/pot_task.c
    firmware/stm32f303/app/src/rom_handler.c
    firmware/stm32f303/app/src/rom_task_support.c
    firmware/stm32f303/app/src/switch_task.c
)

APP_FORMAT_FILES=(
    "${APP_HEADER_FILES[@]}"
    "${APP_SOURCE_FILES[@]}"
)

APP_LINT_FILES=(
    firmware/stm32f303/app/src/button_task.c
    firmware/stm32f303/app/src/effects_model.c
    firmware/stm32f303/app/src/effects_pipeline.c
    firmware/stm32f303/app/src/effects_task.c
    firmware/stm32f303/app/src/i2c_handler.c
    firmware/stm32f303/app/src/peripheral_dispatch.c
    firmware/stm32f303/app/src/pot_task.c
    firmware/stm32f303/app/src/rom_task_support.c
    firmware/stm32f303/app/src/rom_handler.c
    firmware/stm32f303/app/src/switch_task.c
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