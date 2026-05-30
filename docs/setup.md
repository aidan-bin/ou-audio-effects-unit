# Setup

See [Documentation Index](README.md) for all project docs.

## macOS Setup

Recommended local tools on macOS:

- Xcode Command Line Tools
- Homebrew
- CMake
- Ninja
- Python 3
- ARM embedded GCC toolchain (`arm-none-eabi-gcc`)
- STM32CubeMX for the local firmware project
- Optional: STM32CubeProgrammer if you want to flash and debug the target

Typical install commands:

```sh
xcode-select --install
brew install cmake ninja python arm-none-eabi-gcc
```

Then open the local CubeMX project and follow steps to generate code. Only user code should be checked into Git.

## Build Baseline

Run from repository root:

```sh
./scripts/check.sh
```

This runs `./scripts/doctor.sh` first, then build + test.

Optional tooling:

```sh
./scripts/build.sh
./scripts/format.sh
./scripts/format-check.sh
./scripts/lint.sh
./scripts/test.sh
./scripts/clean.sh
./scripts/check-full.sh
./scripts/doctor.sh
```

## Host Demo

Run from repository root:

```sh
./scripts/demo.sh
```

Equivalent explicit commands:

```sh
./scripts/build.sh
python3 -m pip install -r host/python-demo/requirements.txt
python3 host/python-demo/main.py
```

Notes:

- The Python binding in [host/python-demo/effects.py](../host/python-demo/effects.py) resolves the shared library relative to the module location.
- The loader supports libeffects.dll, libeffects.so, and libeffects.dylib.

## Firmware Source Location

The STM32 firmware source is generated locally with CubeMX and is not versioned in this repository.

## Hardware Source Location

- KiCad project and schematics: [hardware/STM32AudioEffects](../hardware/STM32AudioEffects)
- LTspice files: [hardware/LTspice](../hardware/LTspice)
