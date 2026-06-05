#!/usr/bin/env bash

DSP_FORMAT_FILES=()
APP_FORMAT_FILES=()
DSP_LINT_FILES=()
APP_LINT_FILES=()
CUBEMX_USER_CODE_FILES=()

populate_cubemx_user_code_files() {
    find \
        firmware/stm32f303/cubemx/Core \
        firmware/stm32f303/cubemx/USB_DEVICE/App \
        firmware/stm32f303/cubemx/USB_DEVICE/Target \
        -type f \
        \( -name '*.c' -o -name '*.h' \) \
        | LC_ALL=C sort
}

populate_legacy_c_file_lists() {
    DSP_FORMAT_FILES=()
    while IFS= read -r source_file; do
        DSP_FORMAT_FILES+=("$source_file")
    done < <(
        find dsp \
            -type f \
            \( -name '*.c' -o -name '*.h' \) \
            | LC_ALL=C sort
    )

    APP_FORMAT_FILES=()
    while IFS= read -r source_file; do
        APP_FORMAT_FILES+=("$source_file")
    done < <(
        find firmware/stm32f303/app tests host \
            -type f \
            \( -name '*.c' -o -name '*.h' \) \
            | LC_ALL=C sort
    )

    DSP_LINT_FILES=()
    while IFS= read -r source_file; do
        DSP_LINT_FILES+=("$source_file")
    done < <(
        find dsp \
            -type f \
            -name '*.c' \
            | LC_ALL=C sort
    )

    APP_LINT_FILES=()
    while IFS= read -r source_file; do
        APP_LINT_FILES+=("$source_file")
    done < <(
        find firmware/stm32f303/app \
            -type f \
            -name '*.c' \
            | LC_ALL=C sort
    )

    CUBEMX_USER_CODE_FILES=()
    while IFS= read -r source_file; do
        CUBEMX_USER_CODE_FILES+=("$source_file")
    done < <(populate_cubemx_user_code_files)
}

populate_c_file_lists() {
    C_FORMAT_FULL_FILES=()
    while IFS= read -r source_file; do
        C_FORMAT_FULL_FILES+=("$source_file")
    done < <(
        find dsp firmware/stm32f303/app tests host \
            -type f \
            \( -name '*.c' -o -name '*.h' \) \
            ! -path 'firmware/stm32f303/cubemx/*' \
            | LC_ALL=C sort
    )

    C_LINT_FULL_FILES=()
    while IFS= read -r source_file; do
        C_LINT_FULL_FILES+=("$source_file")
    done < <(
        find dsp firmware/stm32f303/app tests host \
            -type f \
            -name '*.c' \
            ! -path 'firmware/stm32f303/cubemx/*' \
            | LC_ALL=C sort
    )

    C_CUBEMX_SECTION_FILES=()
    while IFS= read -r source_file; do
        C_CUBEMX_SECTION_FILES+=("$source_file")
    done < <(populate_cubemx_user_code_files)

    C_CUBEMX_SECTION_LINT_FILES=()
    while IFS= read -r source_file; do
        C_CUBEMX_SECTION_LINT_FILES+=("$source_file")
    done < <(
        find firmware/stm32f303/cubemx/Core firmware/stm32f303/cubemx/USB_DEVICE/App firmware/stm32f303/cubemx/USB_DEVICE/Target \
            -type f \
            -name '*.c' \
            | LC_ALL=C sort
    )
}

populate_legacy_c_file_lists
populate_c_file_lists