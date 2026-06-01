# Setup

## macOS Setup

Install:

- Xcode Command Line Tools
- Homebrew
- CMake
- Ninja
- Python 3
- ARM embedded GCC toolchain (`gcc-arm-embedded` cask)
- STM32CubeMX for the local firmware project
- VS Code extension: STM32 VS Code Extension
- Optional: STM32CubeProgrammer for flash/debug

Commands:

```sh
xcode-select --install
brew install cmake ninja python
brew install --cask gcc-arm-embedded
```

Important:

- Do not rely on Homebrew formula `arm-none-eabi-gcc` alone for firmware builds on macOS; it may be missing newlib/sysroot headers needed by CubeMX-generated sources.
- The `gcc-arm-embedded` cask installs a full toolchain under `/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi`.

Generate firmware from the local CubeMX project. Only user code should be committed.

## Firmware Build (CubeMX CMake)

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

```sh
./scripts/run.sh
```

Runs dependency preflight, then build + tests.

Other common targets:

```sh
./scripts/run.sh build
./scripts/run.sh format-check
./scripts/run.sh lint
./scripts/run.sh test
./scripts/run.sh clean
./scripts/run.sh check-full
./scripts/run.sh doctor
```

## Host Demo

```sh
./scripts/run.sh demo
```

Notes:

- The Python binding in [host/python-demo/effects.py](../host/python-demo/effects.py) loads the shared library relative to the module and supports dll/so/dylib.
