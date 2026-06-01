# Setup

See [Documentation Index](README.md) for all project docs.

## macOS Setup

Recommended local tools on macOS:

- Xcode Command Line Tools
- Homebrew
- CMake
- Ninja
- Python 3
- ARM embedded GCC toolchain (`gcc-arm-embedded` cask)
- STM32CubeMX for the local firmware project
- VS Code extension: STM32 VS Code Extension
- Optional: STM32CubeProgrammer if you want to flash and debug the target

Typical install commands:

```sh
xcode-select --install
brew install cmake ninja python
brew install --cask gcc-arm-embedded
```

Notes:

- Do not rely on Homebrew formula `arm-none-eabi-gcc` alone for firmware builds on macOS; it may be missing newlib/sysroot headers needed by CubeMX-generated sources.
- The `gcc-arm-embedded` cask installs a full toolchain under `/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi`.

Then open the local CubeMX project and follow steps to generate code. Only user code should be checked into Git.

## Firmware Build (CubeMX CMake)

Run from repository root:

```sh
cd firmware/stm32f303/cubemx
cmake --preset Debug
cmake --build --preset Debug
```

Build artifacts are written to:

- `firmware/stm32f303/cubemx/build/Debug/STM32F303RETx-Audio-Effects-Unit.elf`
- `firmware/stm32f303/cubemx/build/Debug/STM32F303RETx-Audio-Effects-Unit.hex`
- `firmware/stm32f303/cubemx/build/Debug/STM32F303RETx-Audio-Effects-Unit.bin`

## Build Baseline

Run from repository root:

```sh
./scripts/run.sh
```

This runs dependency preflight first, then build + test.

Optional tooling:

```sh
./scripts/run.sh build
./scripts/run.sh format
./scripts/run.sh format-check
./scripts/run.sh lint
./scripts/run.sh test
./scripts/run.sh clean
./scripts/run.sh check-full
./scripts/run.sh doctor
```

## Host Demo

Run from repository root:

```sh
./scripts/run.sh demo
```

Equivalent explicit commands:

```sh
./scripts/run.sh build
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
