#!/usr/bin/env bash

run_build() {
	rm -f host/python-demo/libeffects.dll host/python-demo/libeffects.so host/python-demo/libeffects.dylib
	ensure_release_build_tree
	cmake --build build -j
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
	local tool_bin

	tool_bin="$(find_versioned_tool "$tool" || true)"
	if [[ -z "$tool_bin" ]]; then
		echo "$tool not found (tried $(tool_candidates_string "$tool"))"
		exit 1
	fi

	echo "$tool_bin"
}

run_format() {
	local clang_format_bin

	clang_format_bin="$(ensure_clang_tool clang-format)"
	"$clang_format_bin" -i "${DSP_FORMAT_FILES[@]}"
}

run_format_check() {
	local clang_format_bin

	clang_format_bin="$(ensure_clang_tool clang-format)"
	"$clang_format_bin" --dry-run --Werror "${DSP_FORMAT_FILES[@]}"
}

run_lint() {
	local clang_tidy_bin
	local extra_args=()

	clang_tidy_bin="$(ensure_clang_tool clang-tidy)"

	if [[ ! -f build/compile_commands.json ]]; then
		cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null
	fi

	if [[ "$(uname -s)" == "Darwin" ]]; then
		local sdk_path

		sdk_path="$(xcrun --sdk macosx --show-sdk-path)"
		extra_args+=("--extra-arg-before=-isysroot${sdk_path}")
	fi

	run_tidy() {
		"$clang_tidy_bin" -quiet -p build -header-filter='^dsp/' "${extra_args[@]}" "$1" 2>&1 | \
			sed -E '/^[0-9]+ warnings generated\.$/d'
	}

	run_tidy dsp/effects/effects.c
	run_tidy dsp/math/fast_math.c
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