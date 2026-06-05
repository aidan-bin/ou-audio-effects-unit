#!/usr/bin/env bash

USER_CODE_REGIONS_PY="$SCRIPT_DIR/lib/user_code_regions.py"

run_user_code_regions() {
	python3 "$USER_CODE_REGIONS_PY" "$@"
}

user_code_clang_format_args() { run_user_code_regions clang-format-args "$1"; }
user_code_clang_tidy_line_filter() { run_user_code_regions clang-tidy-line-filter "$1"; }

run_build() {
	rm -f host/python-demo/libeffects.dll host/python-demo/libeffects.so host/python-demo/libeffects.dylib
	ensure_release_build_tree
	cmake --build build -j
}

run_build_tests() {
	ensure_release_build_tree
	cmake --build build --target ou_build_tests -j
}

run_build_firmware() {
	ensure_release_build_tree
	cmake --build build --target ou_firmware_build
}

run_build_demo() {
	rm -f host/python-demo/libeffects.dll host/python-demo/libeffects.so host/python-demo/libeffects.dylib
	ensure_release_build_tree
	cmake --build build --target ou_build_demo
}

run_test() {
	ensure_release_build_tree
	cmake --build build -j
	ctest --test-dir build --output-on-failure
}

run_doctor() {
	local missing=0

	check_tool() {
		local tool="$1"
		local hint="$2"

		if ! command -v "$tool" >/dev/null 2>&1; then
			echo "[missing] $tool - $hint"
			missing=1
		else
			echo "[ok] $tool"
		fi
	}

	check_tool_or_versioned() {
		local tool="$1"
		local hint="$2"

		if find_versioned_tool "$tool" >/dev/null; then
			echo "[ok] $tool (or versioned variant)"
		else
			echo "[missing] $tool - $hint"
			missing=1
		fi
	}

	echo "Checking core build and test dependencies..."
	check_tool cmake "Install CMake (e.g. brew install cmake)"
	check_tool ctest "Usually installed with CMake"
	check_tool cc "Install a C compiler toolchain"

	echo
	echo "Checking formatting and lint dependencies..."

	check_tool_or_versioned clang-format "Install LLVM clang-format (e.g. brew install llvm)"
	check_tool_or_versioned clang-tidy "Install LLVM clang-tidy (e.g. brew install llvm)"

	echo
	echo "Checking host demo dependencies..."
	check_tool python3 "Install Python 3"

	if python3 -m pip --version >/dev/null 2>&1; then
		echo "[ok] python3 -m pip"
	else
		echo "[missing] python3 pip module - Install pip for Python 3"
		missing=1
	fi

	if [[ "$missing" -ne 0 ]]; then
		echo
		echo "One or more dependencies are missing."
		exit 1
	fi

	echo
	echo "All required tools were found."
}

ensure_clang_tool() {
	local tool="$1"
	local tool_bin="$(find_versioned_tool "$tool" || true)"
	if [[ -z "$tool_bin" ]]; then
		echo "$tool not found (tried $(tool_candidates_string "$tool"))"
		exit 1
	fi
	echo "$tool_bin"
}

run_clang_format_scope() {
	local clang_format_bin="$1"
	shift

	"$clang_format_bin" "$@" "${DSP_FORMAT_FILES[@]}" "${APP_FORMAT_FILES[@]}"

	for file in "${CUBEMX_USER_CODE_FILES[@]}"; do
		local line_args=()
		local line_arg
		[[ ! -f "$file" ]] && continue

		while IFS= read -r line_arg; do
			[[ -n "$line_arg" ]] && line_args+=("$line_arg")
		done < <(user_code_clang_format_args "$file")
		[[ "${#line_args[@]}" -eq 0 ]] && continue

		"$clang_format_bin" "$@" "${line_args[@]}" "$file"
	done
}

run_format() {
	local clang_format_bin

	clang_format_bin="$(ensure_clang_tool clang-format)"
	run_clang_format_scope "$clang_format_bin" -i
}

run_format_check() {
	local clang_format_bin

	clang_format_bin="$(ensure_clang_tool clang-format)"
	run_clang_format_scope "$clang_format_bin" --dry-run --Werror
}

run_lint() {
	local clang_tidy_bin
	local host_extra_args=()
	local cubemx_extra_args=()
	local root_compile_db_dir="build"
	local cubemx_compile_db_dir=""

	clang_tidy_bin="$(ensure_clang_tool clang-tidy)"

	if [[ ! -f build/compile_commands.json ]]; then
		cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null
	fi

	if [[ "$(uname -s)" == "Darwin" ]]; then
		local sdk_path

		sdk_path="$(xcrun --sdk macosx --show-sdk-path)"
		host_extra_args+=("--extra-arg-before=-isysroot${sdk_path}")
	fi

	compile_commands_contains_file() {
		local compile_db_file="$1"
		local file="$2"
		local absolute_file="$file"
		[[ "$absolute_file" != /* ]] && absolute_file="$REPO_ROOT/$absolute_file"

		grep -Fq "$absolute_file" "$compile_db_file" || grep -Fq "$file" "$compile_db_file"
	}

	resolve_cubemx_compile_db_dir() {
		local candidate

		if [[ -n "${CUBEMX_LINT_BUILD_DIR:-}" ]]; then
			candidate="$CUBEMX_LINT_BUILD_DIR"
			if [[ -f "$candidate/compile_commands.json" ]]; then
				echo "$candidate"
				return 0
			fi

			echo "Configured CUBEMX_LINT_BUILD_DIR has no compile_commands.json: $candidate"
			return 1
		fi

		for candidate in \
			firmware/stm32f303/cubemx/build/Debug \
			firmware/stm32f303/cubemx/build/Release \
			firmware/stm32f303/cubemx/build; do
			if [[ -f "$candidate/compile_commands.json" ]]; then
				echo "$candidate"
				return 0
			fi
		done

		return 1
	}

	configure_cubemx_tidy_extra_args() {
		local compile_db_file="$1"
		local arm_gcc_bin=""
		local include_dir

		arm_gcc_bin="$(grep -m1 -o '/[^ ]*arm-none-eabi-gcc' "$compile_db_file" || true)"
		if [[ -z "$arm_gcc_bin" ]] && command -v arm-none-eabi-gcc >/dev/null 2>&1; then
			arm_gcc_bin="$(command -v arm-none-eabi-gcc)"
		fi

		if [[ -z "$arm_gcc_bin" || ! -x "$arm_gcc_bin" ]]; then
			echo "arm-none-eabi-gcc not found; cannot resolve CubeMX system include paths for clang-tidy."
			return 1
		fi

		for include_dir in \
			"$($arm_gcc_bin -print-file-name=include)" \
			"$($arm_gcc_bin -print-file-name=include-fixed)" \
			"$($arm_gcc_bin -print-file-name=../../../../arm-none-eabi/include)"; do
			if [[ "$include_dir" != /* ]]; then
				include_dir="$(cd "$(dirname "$arm_gcc_bin")" && cd "$include_dir" >/dev/null 2>&1 && pwd || true)"
			fi

			if [[ -d "$include_dir" ]]; then
				cubemx_extra_args+=("--extra-arg-before=-isystem$include_dir")
			fi
		done

		if [[ "${#cubemx_extra_args[@]}" -eq 0 ]]; then
			echo "No valid ARM GCC include directories found for CubeMX lint."
			return 1
		fi
	}

	select_app_compile_db_dir() {
		local file="$1"

		if compile_commands_contains_file "$root_compile_db_dir/compile_commands.json" "$file"; then
			echo "$root_compile_db_dir"
			return 0
		fi

		if [[ -n "$cubemx_compile_db_dir" ]] &&
			compile_commands_contains_file "$cubemx_compile_db_dir/compile_commands.json" "$file"; then
			echo "$cubemx_compile_db_dir"
			return 0
		fi

		return 1
	}

	run_tidy() {
		local file="$1"
		local header_filter="$2"
		local compile_db_dir="$3"
		local compile_db_file="$compile_db_dir/compile_commands.json"
		local line_filter="${4:-}"
		local missing_entry_mode="${5:-skip}"
		local checks_override="${6:-}"
		local tidy_args=(-quiet -p "$compile_db_dir" "-header-filter=$header_filter")
		[[ ! -f "$file" ]] && return 0

		if [[ "$compile_db_dir" == "$root_compile_db_dir" ]]; then
			tidy_args+=("${host_extra_args[@]}")
		fi

		if [[ -n "$cubemx_compile_db_dir" && "$compile_db_dir" == "$cubemx_compile_db_dir" ]]; then
			tidy_args+=("${cubemx_extra_args[@]}")
		fi

		if [[ ! -f "$compile_db_file" ]]; then
			echo "Missing compile database for lint at $compile_db_file"
			return 1
		fi

		if ! compile_commands_contains_file "$compile_db_file" "$file"; then
			if [[ "$missing_entry_mode" == "fail" ]]; then
				echo "Lint requires compile_commands entry for $file in $compile_db_file"
				return 1
			fi

			echo "Skipping lint for $file (no compile_commands entry in $compile_db_file)."
			return 0
		fi

		if [[ -n "$line_filter" ]]; then
			tidy_args+=("-line-filter=$line_filter")
		fi

		if [[ -n "$checks_override" ]]; then
			tidy_args+=("--checks=$checks_override")
		fi

		"$clang_tidy_bin" "${tidy_args[@]}" "$file" 2>&1 | \
			sed -E '/^[0-9]+ warnings generated\.$/d'
	}

	if ! cubemx_compile_db_dir="$(resolve_cubemx_compile_db_dir)"; then
		echo "Missing CubeMX compile database (expected under firmware/stm32f303/cubemx/build or via CUBEMX_LINT_BUILD_DIR)."
		echo "Configure or regenerate the CubeMX build tree to enable USER CODE lint gating."
		return 1
	fi

	if ! configure_cubemx_tidy_extra_args "$cubemx_compile_db_dir/compile_commands.json"; then
		echo "Failed to configure CubeMX clang-tidy include paths from ARM GCC toolchain."
		return 1
	fi

	for file in "${DSP_LINT_FILES[@]}"; do
		run_tidy "$file" '^dsp/' "$root_compile_db_dir"
	done

	for file in "${APP_LINT_FILES[@]}"; do
		local compile_db_dir

		if ! compile_db_dir="$(select_app_compile_db_dir "$file")"; then
			echo "Lint requires compile_commands entry for $file in root or CubeMX compile database."
			return 1
		fi

		run_tidy "$file" '^(dsp|firmware/stm32f303/app)/' "$compile_db_dir"
	done

	for file in "${CUBEMX_USER_CODE_FILES[@]}"; do
		local line_filter
		local line_filter_file
		[[ ! -f "$file" ]] && continue
		[[ "$file" != *.c ]] && continue

		line_filter_file="$file"
		[[ "$line_filter_file" != /* ]] && line_filter_file="$REPO_ROOT/$line_filter_file"

		line_filter="$(user_code_clang_tidy_line_filter "$line_filter_file")"
		[[ -z "$line_filter" ]] && continue

		run_tidy "$file" '^(dsp|firmware/stm32f303/app|firmware/stm32f303/cubemx)/' \
			"$cubemx_compile_db_dir" "$line_filter" fail
	done
}

run_check() {
	if [[ "${SKIP_DOCTOR:-0}" != "1" ]]; then
		run_doctor
	fi

	run_test
}

run_check_full() {
	run_doctor
	run_format_check
	run_lint
	SKIP_DOCTOR=1 run_check
}

run_clean() {
	rm -rf build
	rm -f host/python-demo/libeffects.dll host/python-demo/libeffects.so host/python-demo/libeffects.dylib
	find . -type d -name __pycache__ -prune -exec rm -rf {} +
	find . -type f -name '*.pyc' -delete
}

run_demo() {
	local venv_dir="$REPO_ROOT/.local/python-demo-venv"

	run_build

	if [[ ! -d "$venv_dir" ]]; then
		python3 -m venv "$venv_dir"
	fi

	"$venv_dir/bin/python" -m pip install --upgrade pip
	"$venv_dir/bin/python" -m pip install -r "$REPO_ROOT/host/python-demo/requirements.txt"
	"$venv_dir/bin/python" "$REPO_ROOT/host/python-demo/main.py"
}