#!/usr/bin/env bash

SCOPE_ROOTS=(dsp firmware/stm32f303/app tests host)
CUBEMX_SENTINEL="Core/Src/main.c"
GENERATED_PATH_FRAGMENTS=("/cubemx/" "/nucleo-ou-audio-effects/")
SCOPE_EXCLUDE_PATTERNS=("host/python-demo" "firmware/stm32f303/app/include/test_vector_wav.h")
CUBEMX_USER_CODE_INCLUDE=("Core" "USB_DEVICE" "USB_HOST")
CUBEMX_USER_CODE_EXCLUDE=("build")

discover_cubemx_roots() {
    local firmware_base="firmware/stm32f303"
    [[ ! -d "$firmware_base" ]] && return 0

    find "$firmware_base" -type f -path "*/$CUBEMX_SENTINEL" \
        | sed 's|/'"$CUBEMX_SENTINEL"'$||' \
        | LC_ALL=C sort
}

_is_generated_path() {
    local path="$1"
    local fragment
    for fragment in "${GENERATED_PATH_FRAGMENTS[@]}"; do
        if [[ "$path" == *"$fragment"* ]]; then
            return 0
        fi
    done
    return 1
}

_is_excluded_path() {
    local path="$1"
    local pattern
    for pattern in "${SCOPE_EXCLUDE_PATTERNS[@]}"; do
        if [[ "$path" == "$pattern"* ]]; then
            return 0
        fi
    done
    return 1
}

_discover_in_roots() {
    local roots=()
    local predicates=()
    local parsing_roots=true
    local arg

    for arg in "$@"; do
        if $parsing_roots && [[ "$arg" == "--" ]]; then
            parsing_roots=false
        elif $parsing_roots; then
            roots+=("$arg")
        else
            predicates+=("$arg")
        fi
    done

    local dir
    local found=()

    for dir in "${roots[@]}"; do
        [[ ! -d "$dir" ]] && continue
        while IFS= read -r -d '' file; do
            found+=("$file")
        done < <(
            find "$dir" -type f "${predicates[@]}" -print0 2>/dev/null
        )
    done

    [[ "${#found[@]}" -eq 0 ]] && return 0

    printf '%s\n' "${found[@]}" | LC_ALL=C sort
}

discover_scope() {
    local all_files
    all_files=$(_discover_in_roots "${SCOPE_ROOTS[@]}" -- "$@")

    local line
    while IFS= read -r line; do
        [[ -z "$line" ]] && continue
        _is_generated_path "$line" && continue
        _is_excluded_path "$line" && continue
        echo "$line"
    done <<< "$all_files"
}

discover_scope_c_h() {
    discover_scope \( -name '*.c' -o -name '*.h' \)
}

discover_scope_c() {
    discover_scope -name '*.c'
}

discover_generated_sources() {
    local root
    local tmpfile
    tmpfile=$(mktemp)
    local count=0

    while IFS= read -r root; do
        [[ -z "$root" ]] && continue
        local subdir
        for subdir in "${CUBEMX_USER_CODE_INCLUDE[@]}"; do
            local target="$root/$subdir"
            [[ ! -d "$target" ]] && continue
            local found_tmp
            found_tmp=$(mktemp)
            _discover_in_roots "$target" -- "$@" > "$found_tmp"
            local line
            while IFS= read -r line; do
                [[ -z "$line" ]] && continue
                local rel="${line#$root/}"
                local exclude
                for exclude in "${CUBEMX_USER_CODE_EXCLUDE[@]}"; do
                    if [[ "$rel" == "$exclude"/* || "$rel" == "$exclude" ]]; then
                        continue 2
                    fi
                done
                echo "$line" >> "$tmpfile"
                count=$((count + 1))
            done < "$found_tmp"
            rm -f "$found_tmp"
        done
    done < <(discover_cubemx_roots)

    if [[ "$count" -eq 0 ]]; then
        rm -f "$tmpfile"
        return 0
    fi

    LC_ALL=C sort < "$tmpfile"
    rm -f "$tmpfile"
}

discover_generated_c_h() {
    discover_generated_sources \( -name '*.c' -o -name '*.h' \)
}

discover_generated_c() {
    discover_generated_sources -name '*.c'
}
